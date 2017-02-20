/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Andrey Hristov <andrey@php.net>                              |
  |         Ulf Wendel <uw@php.net>                                      |
  |         Johannes Schlueter <johannes@php.net>                        |
  +----------------------------------------------------------------------+
*/


#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "zend.h"
#include "zend_alloc.h"


#include "zend.h"
#include "zend_alloc.h"
#include "main/php.h"
#include "main/php_streams.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "mysqlnd_ms_enum_n_def.h"

#include "fabric/mysqlnd_fabric.h"
#include "fabric/mysqlnd_fabric_priv.h"

static xmlXPathObjectPtr mysqlnd_fabric_find_value_nodes(xmlDocPtr doc)
{
	xmlXPathObjectPtr retval;
	xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
	if(xpathCtx == NULL) {
		xmlFreeDoc(doc);
		return NULL;
	}

	retval = xmlXPathEvalExpression((xmlChar*)"//params/param/value/array/data/value[3]/array/data/value", xpathCtx);
	xmlXPathFreeContext(xpathCtx);

	return retval;
}

static char *myslqnd_fabric_get_actual_value(char *xpath, xmlXPathContextPtr xpathCtx)
{
	char *retval;
	xmlXPathObjectPtr xpathObj = xpathObj = xmlXPathEvalExpression((xmlChar*)xpath, xpathCtx);

	if (xpathObj == NULL) {
		return NULL;
	}

	retval = (char*)xpathObj->nodesetval->nodeTab[0]->children->content;

	xmlXPathFreeObject(xpathObj);

	return retval;
}

#define GET_VALUE(target, xpath, ctx) \
	(target) = myslqnd_fabric_get_actual_value((xpath), (ctx)); \
	if (!(target)) { \
		xmlXPathFreeContext(ctx); \
		return 1; \
	}

#define COPY_VALUE_IN_FIELDL(target, field, value, value_len) \
	(target)->field ## _len = (value_len); \
	if ((target)->field ## _len > sizeof(server->field) - 1) { \
		xmlXPathFreeContext(xpathCtx); \
		return 1; \
	} \
	strncpy((target)->field, (value), (target)->field ## _len); \
	(target)->field[(target)->field ## _len] = '\0'

#define COPY_VALUE_IN_FIELD(target, field, value) \
	COPY_VALUE_IN_FIELDL(target, field, value, strlen(value))

static int mysqlnd_fabric_fill_server_from_value(xmlNodePtr node, mysqlnd_fabric_server *server)
{
	xmlXPathContextPtr xpathCtx = xmlXPathNewContext((xmlDocPtr)node);
	char *tmp, *port;

	if (xpathCtx == NULL) {
		return 1;
	}
	
	GET_VALUE(tmp, "//array/data/value[1]/string", xpathCtx);
	COPY_VALUE_IN_FIELD(server, uuid, tmp);
	
	GET_VALUE(tmp, "//array/data/value[2]/string", xpathCtx);
	port = strchr(tmp, ':');
	*port = '\0';
	port++;

	COPY_VALUE_IN_FIELDL(server, hostname, tmp, (port - tmp) - 1);
	server->port = atoi(port);

	GET_VALUE(tmp, "//array/data/value[3]/boolean", xpathCtx);

	switch (tmp[0]) {
	case '0': server->mode = READ_ONLY; break;
	case '1': server->mode = READ_WRITE; break;
	default:
		xmlXPathFreeContext(xpathCtx);
		return 1;
	}
	
	server->role = SPARE; /* FIXME - currently role is ignored */
	server->weight = 1.0;

	xmlXPathFreeContext(xpathCtx);

	return 0;
}

mysqlnd_fabric_server *mysqlnd_fabric_parse_xml(mysqlnd_fabric *fabric, char *xmlstr, int xmlstr_len)
{
	mysqlnd_fabric_server *retval;
	xmlDocPtr doc;
	xmlXPathObjectPtr xpathObj1;
	int i;

	LIBXML_TEST_VERSION
	doc = xmlParseMemory(xmlstr, xmlstr_len);

	if (doc == NULL) {
		SET_FABRIC_ERROR(*fabric, 2000, "HY000", "Failed to parse Fabric XML reply");
		return NULL;
	}

	xpathObj1 = mysqlnd_fabric_find_value_nodes(doc);
	if (!xpathObj1) {
		xmlFreeDoc(doc);
		SET_FABRIC_ERROR(*fabric, 2000, "HY000", "Failed to find nodes in Fabric XML reply");
		return NULL;
	}

	if (!xpathObj1->nodesetval) {
		/* Verbose debug info in /methodresponse/params/param/value/array/data/value[2]/array/data/value[3]/struct/member/value/string */
		xmlXPathFreeObject(xpathObj1);
		xmlFreeDoc(doc);
		SET_FABRIC_ERROR(*fabric, 2000, "HY000", "Failed to find node set in Fabric XML reply");
		return NULL;
	}

	retval = safe_emalloc(xpathObj1->nodesetval->nodeNr+1, sizeof(mysqlnd_fabric_server), 0);
	for (i = 0; i < xpathObj1->nodesetval->nodeNr; i++) {
		if (mysqlnd_fabric_fill_server_from_value(xpathObj1->nodesetval->nodeTab[i], &retval[i])) {
			xmlXPathFreeObject(xpathObj1);
			xmlFreeDoc(doc);
			SET_FABRIC_ERROR(*fabric, 2000, "HY000", "Failed to parse node entry in Fabric XML reply");
			return NULL;
		}
	}

	retval[i].hostname_len = 0;
	retval[i].hostname[0] = '\0';
	retval[i].port = 0;

	xmlXPathFreeObject(xpathObj1);
	xmlFreeDoc(doc);

	return retval;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
