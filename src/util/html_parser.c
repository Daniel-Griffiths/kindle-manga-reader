#include "html_parser.h"
#include <string.h>

htmlDocPtr html_parse(const char *html, size_t len) {
    return htmlReadMemory(html, (int)len, NULL, "UTF-8",
                          HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                          HTML_PARSE_NOWARNING);
}

GPtrArray *html_xpath(htmlDocPtr doc, const char *expr) {
    GPtrArray *results = g_ptr_array_new();
    if (!doc) return results;

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) return results;

    xmlXPathObjectPtr obj = xmlXPathEvalExpression(
        (const xmlChar *)expr, ctx);
    if (!obj) {
        xmlXPathFreeContext(ctx);
        return results;
    }

    if (obj->nodesetval) {
        for (int i = 0; i < obj->nodesetval->nodeNr; i++) {
            g_ptr_array_add(results, obj->nodesetval->nodeTab[i]);
        }
    }

    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctx);
    return results;
}

char *html_node_text(xmlNodePtr node) {
    xmlChar *content = xmlNodeGetContent(node);
    if (!content) return g_strdup("");
    char *result = g_strdup((const char *)content);
    xmlFree(content);
    /* Trim whitespace */
    g_strstrip(result);
    return result;
}

char *html_node_attr(xmlNodePtr node, const char *attr) {
    xmlChar *val = xmlGetProp(node, (const xmlChar *)attr);
    if (!val) return NULL;
    char *result = g_strdup((const char *)val);
    xmlFree(val);
    return result;
}
