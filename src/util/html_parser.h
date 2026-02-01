#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <glib.h>

/* Parse an HTML string into an xmlDoc. Caller must xmlFreeDoc(). */
htmlDocPtr  html_parse(const char *html, size_t len);

/* Run an XPath query, return array of xmlNode*. Caller frees the GPtrArray
 * (but not the nodes themselves — they belong to the doc). */
GPtrArray  *html_xpath(htmlDocPtr doc, const char *expr);

/* Get text content of a node (caller must g_free). */
char       *html_node_text(xmlNodePtr node);

/* Get an attribute value (caller must g_free). */
char       *html_node_attr(xmlNodePtr node, const char *attr);

#endif /* HTML_PARSER_H */
