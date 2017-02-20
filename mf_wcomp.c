/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2008 The PHP Group                                |
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
#include "php.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "mysqlnd_ms.h"

/* $Id: mysqlnd_ms_config_json.c 311386 2011-05-24 12:10:21Z andrey $ */



/* {{{ mysqlnd_ms_match_wild */
PHP_MYSQLND_MS_API
zend_bool mysqlnd_ms_match_wild(const char * const str, const char * const wildstr TSRMLS_DC)
{
	static char many = '%';
	static char single = '_';
	static char escape = '\\';
	const char * s = str;
	const char * w = wildstr;

	DBG_ENTER("mysqlnd_ms_match_wild");
	/* check for */
	if (!s || !w) {
		DBG_RETURN(FALSE);
	}
	do {
		while (*w != many && *w != single) {
			if (*w == escape && !*++w) {
				DBG_RETURN(FALSE);
			}
			if (*s != *w) {
				DBG_RETURN(FALSE);
			} else if (!*s) {
				/* the same chars, and both are \0 terminators */
				DBG_RETURN(TRUE);
			}
			/* still not the end */
			++s;
			++w;
		}
		/* one or many */
		if (*w == many) {
			/* even if *s is \0 this is ok */
			DBG_RETURN(TRUE);
		} else if (*w == single) {
			if (!*s) {
				/* single is not zero */
				DBG_RETURN(FALSE);
			}
			++s;
			++w;
		}
	} while (1);
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
