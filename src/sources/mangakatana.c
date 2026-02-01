#include "mangakatana.h"
#include "../net/http.h"
#include "../util/html_parser.h"
#include <string.h>
#include <stdlib.h>

#define MK_BASE "https://mangakatana.com"

static MangaList *mk_search(MangaSource *self, const char *query) {
    (void)self;
    MangaList *list = manga_list_new();

    char *encoded = g_uri_escape_string(query, NULL, FALSE);
    char *url = g_strdup_printf("%s/?search=%s&search_by=book_name",
                                MK_BASE, encoded);
    g_free(encoded);

    HttpResponse *resp = http_get(url);
    g_free(url);
    if (!resp || resp->status_code != 200) {
        http_response_free(resp);
        return list;
    }

    htmlDocPtr doc = html_parse(resp->data, resp->size);
    http_response_free(resp);
    if (!doc) return list;

    /* Each result is in div.item inside #book_list */
    GPtrArray *items = html_xpath(doc, "//div[@id='book_list']//div[contains(@class,'item')]");

    for (unsigned int i = 0; i < items->len; i++) {
        xmlNodePtr item_node = g_ptr_array_index(items, i);

        /* Build sub-xpath relative to this node by finding child elements */
        /* Title + URL from h3 > a */
        xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
        ctx->node = item_node;
        xmlXPathObjectPtr title_obj = xmlXPathEvalExpression(
            (const xmlChar *)".//h3[contains(@class,'title')]//a", ctx);

        char *title = NULL;
        char *item_url = NULL;

        if (title_obj && title_obj->nodesetval && title_obj->nodesetval->nodeNr > 0) {
            xmlNodePtr a = title_obj->nodesetval->nodeTab[0];
            title = html_node_text(a);
            item_url = html_node_attr(a, "href");
        }
        xmlXPathFreeObject(title_obj);

        /* Cover from img */
        xmlXPathObjectPtr img_obj = xmlXPathEvalExpression(
            (const xmlChar *)".//img", ctx);
        char *cover = NULL;
        if (img_obj && img_obj->nodesetval && img_obj->nodesetval->nodeNr > 0) {
            cover = html_node_attr(img_obj->nodesetval->nodeTab[0], "src");
        }
        xmlXPathFreeObject(img_obj);
        xmlXPathFreeContext(ctx);

        if (title && item_url) {
            MangaListItem *mi = manga_list_item_new();
            mi->title = title;
            mi->url = item_url;
            mi->cover_url = cover;
            g_ptr_array_add(list->items, mi);
        } else {
            g_free(title);
            g_free(item_url);
            g_free(cover);
        }
    }

    g_ptr_array_free(items, TRUE);
    xmlFreeDoc(doc);
    return list;
}

static Manga *mk_get_details(MangaSource *self, const char *url) {
    (void)self;
    HttpResponse *resp = http_get(url);
    if (!resp || resp->status_code != 200) {
        http_response_free(resp);
        return NULL;
    }

    htmlDocPtr doc = html_parse(resp->data, resp->size);
    http_response_free(resp);
    if (!doc) return NULL;

    Manga *manga = manga_new();
    manga->url = g_strdup(url);

    /* Title */
    GPtrArray *nodes = html_xpath(doc, "//h1[contains(@class,'heading')]");
    if (nodes->len > 0) {
        manga->title = html_node_text(g_ptr_array_index(nodes, 0));
    }
    g_ptr_array_free(nodes, TRUE);

    /* Cover image */
    nodes = html_xpath(doc, "//div[contains(@class,'cover')]//img");
    if (nodes->len > 0) {
        manga->cover_url = html_node_attr(g_ptr_array_index(nodes, 0), "src");
    }
    g_ptr_array_free(nodes, TRUE);

    /* Status */
    nodes = html_xpath(doc, "//div[contains(@class,'status')]");
    if (nodes->len > 0) {
        manga->status = html_node_text(g_ptr_array_index(nodes, 0));
    }
    g_ptr_array_free(nodes, TRUE);

    /* Description */
    nodes = html_xpath(doc, "//div[contains(@class,'summary')]//p");
    if (nodes->len > 0) {
        manga->description = html_node_text(g_ptr_array_index(nodes, 0));
    } else {
        g_ptr_array_free(nodes, TRUE);
        nodes = html_xpath(doc, "//div[contains(@class,'summary')]");
        if (nodes->len > 0) {
            manga->description = html_node_text(g_ptr_array_index(nodes, 0));
        }
    }
    g_ptr_array_free(nodes, TRUE);

    /* Author */
    nodes = html_xpath(doc, "//a[contains(@class,'author')]");
    if (nodes->len > 0) {
        manga->author = html_node_text(g_ptr_array_index(nodes, 0));
    }
    g_ptr_array_free(nodes, TRUE);

    /* Chapters */
    nodes = html_xpath(doc, "//div[contains(@class,'chapters')]//tr//a");
    for (unsigned int i = 0; i < nodes->len; i++) {
        xmlNodePtr a = g_ptr_array_index(nodes, i);
        Chapter *ch = chapter_new();
        ch->title = html_node_text(a);
        ch->url = html_node_attr(a, "href");
        ch->number = (int)(nodes->len - i);
        g_ptr_array_add(manga->chapters, ch);
    }
    g_ptr_array_free(nodes, TRUE);

    xmlFreeDoc(doc);
    return manga;
}

static PageList *mk_get_pages(MangaSource *self, const char *chapter_url) {
    (void)self;
    PageList *pages = page_list_new();

    HttpResponse *resp = http_get(chapter_url);
    if (!resp || resp->status_code != 200) {
        http_response_free(resp);
        return pages;
    }

    /*
     * MangaKatana embeds page URLs in a JS variable like:
     *   var thzq = ['url1','url2',...];
     * We parse this out with simple string scanning.
     */
    const char *marker = "var thzq";
    char *pos = strstr(resp->data, marker);
    if (!pos) {
        /* Try alternate variable names used by the site */
        marker = "var ytaw";
        pos = strstr(resp->data, marker);
    }
    if (!pos) {
        marker = "var hpiw";
        pos = strstr(resp->data, marker);
    }

    if (pos) {
        /* Find the opening bracket */
        char *bracket = strchr(pos, '[');
        if (bracket) {
            char *end = strchr(bracket, ']');
            if (end) {
                /* Extract URLs between quotes */
                char *p = bracket + 1;
                while (p < end) {
                    char *q1 = strchr(p, '\'');
                    if (!q1 || q1 >= end) {
                        q1 = strchr(p, '"');
                    }
                    if (!q1 || q1 >= end) break;

                    char quote = *q1;
                    char *q2 = strchr(q1 + 1, quote);
                    if (!q2 || q2 >= end) break;

                    char *img_url = g_strndup(q1 + 1, (gsize)(q2 - q1 - 1));
                    g_ptr_array_add(pages->image_urls, img_url);
                    p = q2 + 1;
                }
            }
        }
    }

    http_response_free(resp);
    return pages;
}

static MangaList *mk_get_latest(MangaSource *self) {
    (void)self;
    MangaList *list = manga_list_new();

    HttpResponse *resp = http_get(MK_BASE);
    if (!resp || resp->status_code != 200) {
        http_response_free(resp);
        return list;
    }

    htmlDocPtr doc = html_parse(resp->data, resp->size);
    http_response_free(resp);
    if (!doc) return list;

    GPtrArray *items = html_xpath(doc,
        "//div[@id='book_list']//div[contains(@class,'item')]//h3//a");

    for (unsigned int i = 0; i < items->len && i < 20; i++) {
        xmlNodePtr a = g_ptr_array_index(items, i);
        MangaListItem *mi = manga_list_item_new();
        mi->title = html_node_text(a);
        mi->url = html_node_attr(a, "href");
        g_ptr_array_add(list->items, mi);
    }

    g_ptr_array_free(items, TRUE);
    xmlFreeDoc(doc);
    return list;
}

static void mk_destroy(MangaSource *self) {
    g_free(self);
}

MangaSource *mangakatana_source_new(void) {
    MangaSource *s = g_new0(MangaSource, 1);
    s->name = "MangaKatana";
    s->base_url = MK_BASE;
    s->search = mk_search;
    s->get_manga_details = mk_get_details;
    s->get_chapter_pages = mk_get_pages;
    s->get_latest = mk_get_latest;
    s->destroy = mk_destroy;
    return s;
}
