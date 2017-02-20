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
  | Authors: Andrey Hristov <andrey@mysql.com>                           |
  |          Ulf Wendel <uwendel@mysql.com>                              |
  +----------------------------------------------------------------------+
*/

/* $Id: mysqlnd_qp.h 331418 2013-09-17 11:57:46Z ab $ */

#ifndef MYSQLND_MS_TOKENIZE_H
#define MYSQLND_MS_TOKENIZE_H

PHP_MYSQLND_MS_API void mysqlnd_qp_free_scanner(struct st_mysqlnd_query_scanner * scanner TSRMLS_DC);
PHP_MYSQLND_MS_API struct st_mysqlnd_query_scanner * mysqlnd_qp_create_scanner(TSRMLS_D);
PHP_MYSQLND_MS_API struct st_ms_token_and_value mysqlnd_qp_get_token(struct st_mysqlnd_query_scanner * scanner TSRMLS_DC);
PHP_MYSQLND_MS_API void mysqlnd_qp_set_string(struct st_mysqlnd_query_scanner * scanner, const char * const s, size_t len TSRMLS_DC);

#ifdef MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION
PHP_MYSQLND_MS_API struct st_mysqlnd_query_parser * mysqlnd_qp_create_parser(TSRMLS_D);
PHP_MYSQLND_MS_API void mysqlnd_qp_free_parser(struct st_mysqlnd_query_parser * parser TSRMLS_DC);
PHP_MYSQLND_MS_API int mysqlnd_qp_start_parser(struct st_mysqlnd_query_parser * parser, const char * const query, const size_t query_len TSRMLS_DC);
#endif

#endif /* MYSQLND_MS_TOKENIZE_H */
