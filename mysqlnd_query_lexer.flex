%{
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

/*
Compile with : flex mysqlnd_query_lexer.flex
*/

#include <string.h>
#include "php.h"
#include "php_ini.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#include "mysqlnd_ms.h"
#include "mysqlnd_query_parser.h"

int old_yystate;
#define yyerror mysqlnd_qp_error
int mysqlnd_qp_error(const char *format, ...);

#define YY_DECL int mysqlnd_qp_lex(YYSTYPE * yylval_param, yyscan_t yyscanner TSRMLS_DC)

#define YY_NO_INPUT
#define YY_NO_UNISTD_H

/* In Unicode . is [\0-\x7F] | [\xC2-\xDF][\x80-\xBF] | \xE0[\xA0-\xBF][\x80-\xBF] | [\xE1-\xEF][\x80-\xBF][\x80-\xBF] */

%}

%option 8bit
%option prefix="mysqlnd_qp_"
%option outfile="mysqlnd_query_lexer.c"
%option reentrant noyywrap nounput
%option extra-type="zval *"
%option bison-bridge
%option header-file="mysqlnd_query_lexer.lex.h"

%x COMMENT_MODE
%s BETWEEN_MODE

%%
%{
	/* can't use `yylval` here because `yylval` is initialized by flex to `yylval_param` later */
	zval * token_value = &yylval_param->zv;
	const char ** kn = &(yylval_param->kn);
	smart_str ** comment = &(yylval_param->comment);
	DBG_ENTER("my_lex_routine");
%}

(?i:ACCESSIBLE)						{ *kn = yytext; DBG_INF("QC_TOKEN_ACCESSIBLE");		DBG_RETURN(QC_TOKEN_ACCESSIBLE); }
(?i:ACTION)							{ *kn = yytext; DBG_INF("QC_TOKEN_ACTION");			DBG_RETURN(QC_TOKEN_ACTION); }
(?i:ADD)							{ *kn = yytext; DBG_INF("QC_TOKEN_ADD");				DBG_RETURN(QC_TOKEN_ADD); }
(?i:ADDDATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_ADDDATE");			DBG_RETURN(QC_TOKEN_ADDDATE); }
(?i:AFTER)							{ *kn = yytext; DBG_INF("QC_TOKEN_AFTER");			DBG_RETURN(QC_TOKEN_AFTER); }
(?i:AGAINST)						{ *kn = yytext; DBG_INF("QC_TOKEN_AGAINST");			DBG_RETURN(QC_TOKEN_AGAINST); }
(?i:AGGREGATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_AGGREGATE");		DBG_RETURN(QC_TOKEN_AGGREGATE); }
(?i:ALGORITHM)						{ *kn = yytext; DBG_INF("QC_TOKEN_ALGORITHM");		DBG_RETURN(QC_TOKEN_ALGORITHM); }
(?i:ALL)							{ *kn = yytext; DBG_INF("QC_TOKEN_ALL");				DBG_RETURN(QC_TOKEN_ALL); }
(?i:ALTER)							{ *kn = yytext; DBG_INF("QC_TOKEN_ALTER");			DBG_RETURN(QC_TOKEN_ALTER); }
(?i:ANALYZE)						{ *kn = yytext; DBG_INF("QC_TOKEN_ANALYZE");			DBG_RETURN(QC_TOKEN_ANALYZE); }
<BETWEEN_MODE>AND					{ BEGIN INITIAL; *kn = yytext; DBG_INF("QC_TOKEN_BETWEEN_AND");	DBG_RETURN(QC_TOKEN_BETWEEN_AND); }
(?i:AND)							{ *kn = yytext; DBG_INF("QC_TOKEN_AND");				DBG_RETURN(QC_TOKEN_AND); }
(?i:ANY)							{ *kn = yytext; DBG_INF("QC_TOKEN_ANY");				DBG_RETURN(QC_TOKEN_ANY); }
(?i:AS)								{ *kn = yytext; DBG_INF("QC_TOKEN_AS");				DBG_RETURN(QC_TOKEN_AS); }
(?i:ASC)							{ *kn = yytext; DBG_INF("QC_TOKEN_ASC");				DBG_RETURN(QC_TOKEN_ASC); }
(?i:ASCII)							{ *kn = yytext; DBG_INF("QC_TOKEN_ASCII");			DBG_RETURN(QC_TOKEN_ASCII); }
(?i:ASENSITIVE)						{ *kn = yytext; DBG_INF("QC_TOKEN_ASENSITIVE");		DBG_RETURN(QC_TOKEN_ASENSITIVE); }
(?i:AT)								{ *kn = yytext; DBG_INF("QC_TOKEN_AT");				DBG_RETURN(QC_TOKEN_AT); }
(?i:AUTHORS)						{ *kn = yytext; DBG_INF("QC_TOKEN_AUTHORS");			DBG_RETURN(QC_TOKEN_AUTHORS); }
(?i:AUTOEXTEND_SIZE)				{ *kn = yytext; DBG_INF("QC_TOKEN_AUTOEXTEND_SIZE");	DBG_RETURN(QC_TOKEN_AUTOEXTEND_SIZE); }
(?i:AUTO_INC)						{ *kn = yytext; DBG_INF("QC_TOKEN_AUTO_INC");			DBG_RETURN(QC_TOKEN_AUTO_INC); }
(?i:AVG_ROW_LENGTH)					{ *kn = yytext; DBG_INF("QC_TOKEN_AVG_ROW_LENGTH");	DBG_RETURN(QC_TOKEN_AVG_ROW_LENGTH); }
(?i:AVG)							{ *kn = yytext; DBG_INF("QC_TOKEN_AVG");				DBG_RETURN(QC_TOKEN_AVG); }
(?i:BACKUP)							{ *kn = yytext; DBG_INF("QC_TOKEN_BACKUP");			DBG_RETURN(QC_TOKEN_BACKUP); }
(?i:BEFORE)							{ *kn = yytext; DBG_INF("QC_TOKEN_BEFORE");			DBG_RETURN(QC_TOKEN_BEFORE); }
(?i:BEGIN)							{ *kn = yytext; DBG_INF("QC_TOKEN_BEGIN");			DBG_RETURN(QC_TOKEN_BEGIN); }
(?i:BETWEEN)						{ BEGIN BETWEEN_MODE; *kn = yytext; DBG_INF("QC_TOKEN_BETWEEN");			DBG_RETURN(QC_TOKEN_BETWEEN); }
(?i:BIGINT)							{ *kn = yytext; DBG_INF("QC_TOKEN_BIGINT");			DBG_RETURN(QC_TOKEN_BIGINT); }
(?i:BINARY)							{ *kn = yytext; DBG_INF("QC_TOKEN_BINARY");			DBG_RETURN(QC_TOKEN_BINARY); }
(?i:BINLOG)							{ *kn = yytext; DBG_INF("QC_TOKEN_BINLOG");			DBG_RETURN(QC_TOKEN_BINLOG); }
(?i:BIT)							{ *kn = yytext; DBG_INF("QC_TOKEN_BIT");				DBG_RETURN(QC_TOKEN_BIT); }
(?i:BLOB)							{ *kn = yytext; DBG_INF("QC_TOKEN_BLOB");				DBG_RETURN(QC_TOKEN_BLOB); }
(?i:BLOCK)							{ *kn = yytext; DBG_INF("QC_TOKEN_BLOCK");			DBG_RETURN(QC_TOKEN_BLOCK); }
(?i:BOOLEAN)						{ *kn = yytext; DBG_INF("QC_TOKEN_BOOLEAN");			DBG_RETURN(QC_TOKEN_BOOLEAN); }
(?i:BOOL)							{ *kn = yytext; DBG_INF("QC_TOKEN_BOOL");				DBG_RETURN(QC_TOKEN_BOOL); }
(?i:BOTH)							{ *kn = yytext; DBG_INF("QC_TOKEN_BOTH");				DBG_RETURN(QC_TOKEN_BOTH); }
(?i:BTREE)							{ *kn = yytext; DBG_INF("QC_TOKEN_BTREE");			DBG_RETURN(QC_TOKEN_BTREE); }
(?i:BY)								{ *kn = yytext; DBG_INF("QC_TOKEN_BY");				DBG_RETURN(QC_TOKEN_BY); }
(?i:BYTE)							{ *kn = yytext; DBG_INF("QC_TOKEN_BYTE");				DBG_RETURN(QC_TOKEN_BYTE); }
(?i:CACHE)							{ *kn = yytext; DBG_INF("QC_TOKEN_CACHE");			DBG_RETURN(QC_TOKEN_CACHE); }
(?i:CALL)							{ *kn = yytext; DBG_INF("QC_TOKEN_CALL");				DBG_RETURN(QC_TOKEN_CALL); }
(?i:CASCADE)						{ *kn = yytext; DBG_INF("QC_TOKEN_CASCADE");			DBG_RETURN(QC_TOKEN_CASCADE); }
(?i:CASCADED)						{ *kn = yytext; DBG_INF("QC_TOKEN_CASCADED");			DBG_RETURN(QC_TOKEN_CASCADED); }
(?i:CASE)							{ *kn = yytext; DBG_INF("QC_TOKEN_CASE");				DBG_RETURN(QC_TOKEN_CASE); }
(?i:CAST)							{ *kn = yytext; DBG_INF("QC_TOKEN_CAST");				DBG_RETURN(QC_TOKEN_CAST); }
(?i:CATALOG_NAME)					{ *kn = yytext; DBG_INF("QC_TOKEN_CATALOG_NAME");		DBG_RETURN(QC_TOKEN_CATALOG_NAME); }
(?i:CHAIN)							{ *kn = yytext; DBG_INF("QC_TOKEN_CHAIN");			DBG_RETURN(QC_TOKEN_CHAIN); }
(?i:CHANGE)							{ *kn = yytext; DBG_INF("QC_TOKEN_CHANGE");			DBG_RETURN(QC_TOKEN_CHANGE); }
(?i:CHANGED)						{ *kn = yytext; DBG_INF("QC_TOKEN_CHANGED");			DBG_RETURN(QC_TOKEN_CHANGED); }
(?i:CHARSET)						{ *kn = yytext; DBG_INF("QC_TOKEN_CHARSET");			DBG_RETURN(QC_TOKEN_CHARSET); }
(?i:CHAR)							{ *kn = yytext; DBG_INF("QC_TOKEN_CHAR");				DBG_RETURN(QC_TOKEN_CHAR); }
(?i:CHECKSUM)						{ *kn = yytext; DBG_INF("QC_TOKEN_CHECKSUM");			DBG_RETURN(QC_TOKEN_CHECKSUM); }
(?i:CHECK)							{ *kn = yytext; DBG_INF("QC_TOKEN_CHECK");			DBG_RETURN(QC_TOKEN_CHECK); }
(?i:CIPHER)							{ *kn = yytext; DBG_INF("QC_TOKEN_CIPHER");			DBG_RETURN(QC_TOKEN_CIPHER); }
(?i:CLASS_ORIGIN)					{ *kn = yytext; DBG_INF("QC_TOKEN_CLASS_ORIGIN");		DBG_RETURN(QC_TOKEN_CLASS_ORIGIN); }
(?i:CLIENT)							{ *kn = yytext; DBG_INF("QC_TOKEN_CLIENT");			DBG_RETURN(QC_TOKEN_CLIENT); }
(?i:CLOSE)							{ *kn = yytext; DBG_INF("QC_TOKEN_CLOSE");			DBG_RETURN(QC_TOKEN_CLOSE); }
(?i:COALESCE)						{ *kn = yytext; DBG_INF("QC_TOKEN_COALESCE");			DBG_RETURN(QC_TOKEN_COALESCE); }
(?i:CODE)							{ *kn = yytext; DBG_INF("QC_TOKEN_CODE");				DBG_RETURN(QC_TOKEN_CODE); }
(?i:COLLATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_COLLATE");			DBG_RETURN(QC_TOKEN_COLLATE); }
(?i:COLLATION)						{ *kn = yytext; DBG_INF("QC_TOKEN_COLLATION");		DBG_RETURN(QC_TOKEN_COLLATION); }
(?i:COLUMNS)						{ *kn = yytext; DBG_INF("QC_TOKEN_COLUMNS");			DBG_RETURN(QC_TOKEN_COLUMNS); }
(?i:COLUMN)							{ *kn = yytext; DBG_INF("QC_TOKEN_COLUMN");			DBG_RETURN(QC_TOKEN_COLUMN); }
(?i:COLUMN_NAME)					{ *kn = yytext; DBG_INF("QC_TOKEN_COLUMN_NAME");		DBG_RETURN(QC_TOKEN_COLUMN_NAME); }
(?i:COMMENT)						{ *kn = yytext; DBG_INF("QC_TOKEN_COMMENT");			DBG_RETURN(QC_TOKEN_COMMENT); }
(?i:COMMITTED)						{ *kn = yytext; DBG_INF("QC_TOKEN_COMMITTED");		DBG_RETURN(QC_TOKEN_COMMITTED); }
(?i:COMMIT)							{ *kn = yytext; DBG_INF("QC_TOKEN_COMMIT");			DBG_RETURN(QC_TOKEN_COMMIT); }
(?i:COMPACT)						{ *kn = yytext; DBG_INF("QC_TOKEN_COMPACT");			DBG_RETURN(QC_TOKEN_COMPACT); }
(?i:COMPLETION)						{ *kn = yytext; DBG_INF("QC_TOKEN_COMPLETION");		DBG_RETURN(QC_TOKEN_COMPLETION); }
(?i:COMPRESSED)						{ *kn = yytext; DBG_INF("QC_TOKEN_COMPRESSED");		DBG_RETURN(QC_TOKEN_COMPRESSED); }
(?i:CONCURRENT)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONCURRENT");		DBG_RETURN(QC_TOKEN_CONCURRENT); }
(?i:CONDITION)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONDITION");		DBG_RETURN(QC_TOKEN_CONDITION); }
(?i:CONNECTION)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONNECTION");		DBG_RETURN(QC_TOKEN_CONNECTION); }
(?i:CONSISTENT)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONSISTENT");		DBG_RETURN(QC_TOKEN_CONSISTENT); }
(?i:CONSTRAINT)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONSTRAINT");		DBG_RETURN(QC_TOKEN_CONSTRAINT); }
(?i:CONSTRAINT_CATALOG)				{ *kn = yytext; DBG_INF("QC_TOKEN_CONSTRAINT_CATALOG");DBG_RETURN(QC_TOKEN_CONSTRAINT_CATALOG); }
(?i:CONSTRAINT_NAME)				{ *kn = yytext; DBG_INF("QC_TOKEN_CONSTRAINT_NAME");	DBG_RETURN(QC_TOKEN_CONSTRAINT_NAME); }
(?i:CONSTRAINT_SCHEMA)				{ *kn = yytext; DBG_INF("QC_TOKEN_CONSTRAINT_SCHEMA");DBG_RETURN(QC_TOKEN_CONSTRAINT_SCHEMA); }
(?i:CONTAINS)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONTAINS");			DBG_RETURN(QC_TOKEN_CONTAINS); }
(?i:CONTEXT)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONTEXT");			DBG_RETURN(QC_TOKEN_CONTEXT); }
(?i:CONTINUE)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONTINUE");			DBG_RETURN(QC_TOKEN_CONTINUE); }
(?i:CONTRIBUTORS)					{ *kn = yytext; DBG_INF("QC_TOKEN_CONTRIBUTORS");		DBG_RETURN(QC_TOKEN_CONTRIBUTORS); }
(?i:CONVERT)						{ *kn = yytext; DBG_INF("QC_TOKEN_CONVERT");			DBG_RETURN(QC_TOKEN_CONVERT); }
(?i:COUNT)							{ *kn = yytext; DBG_INF("QC_TOKEN_COUNT");			DBG_RETURN(QC_TOKEN_COUNT); }
(?i:CPU)							{ *kn = yytext; DBG_INF("QC_TOKEN_CPU");				DBG_RETURN(QC_TOKEN_CPU); }
(?i:CREATE)							{ *kn = yytext; DBG_INF("QC_TOKEN_CREATE");			DBG_RETURN(QC_TOKEN_CREATE); }
(?i:CROSS)							{ *kn = yytext; DBG_INF("QC_TOKEN_CROSS");			DBG_RETURN(QC_TOKEN_CROSS); }
(?i:CUBE)							{ *kn = yytext; DBG_INF("QC_TOKEN_CUBE");				DBG_RETURN(QC_TOKEN_CUBE); }
(?i:CURDATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_CURDATE");			DBG_RETURN(QC_TOKEN_CURDATE); }
(?i:CURRENT_USER)					{ *kn = yytext; DBG_INF("QC_TOKEN_CURRENT_USER");		DBG_RETURN(QC_TOKEN_CURRENT_USER); }
(?i:CURSOR)							{ *kn = yytext; DBG_INF("QC_TOKEN_CURSOR");			DBG_RETURN(QC_TOKEN_CURSOR); }
(?i:CURSOR_NAME)					{ *kn = yytext; DBG_INF("QC_TOKEN_CURSOR_NAME");		DBG_RETURN(QC_TOKEN_CURSOR_NAME); }
(?i:CURTIME)						{ *kn = yytext; DBG_INF("QC_TOKEN_CURTIME");			DBG_RETURN(QC_TOKEN_CURTIME); }
(?i:DATABASE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DATABASE");			DBG_RETURN(QC_TOKEN_DATABASE); }
(?i:DATABASES)						{ *kn = yytext; DBG_INF("QC_TOKEN_DATABASES");		DBG_RETURN(QC_TOKEN_DATABASES); }
(?i:DATAFILE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DATAFILE");			DBG_RETURN(QC_TOKEN_DATAFILE); }
(?i:DATA)							{ *kn = yytext; DBG_INF("QC_TOKEN_DATA");				DBG_RETURN(QC_TOKEN_DATA); }
(?i:DATETIME)						{ *kn = yytext; DBG_INF("QC_TOKEN_DATETIME");			DBG_RETURN(QC_TOKEN_DATETIME); }
(?i:DATE_ADD_INTERVAL)				{ *kn = yytext; DBG_INF("QC_TOKEN_DATE_ADD_INTERVAL");DBG_RETURN(QC_TOKEN_DATE_ADD_INTERVAL); }
(?i:DATE_SUB_INTERVAL)				{ *kn = yytext; DBG_INF("QC_TOKEN_DATE_SUB_INTERVAL");DBG_RETURN(QC_TOKEN_DATE_SUB_INTERVAL); }
(?i:DATE)							{ *kn = yytext; DBG_INF("QC_TOKEN_DATE");				DBG_RETURN(QC_TOKEN_DATE); }
(?i:DAY_HOUR)						{ *kn = yytext; DBG_INF("QC_TOKEN_DAY_HOUR");			DBG_RETURN(QC_TOKEN_DAY_HOUR); }
(?i:DAY_MICROSECOND)				{ *kn = yytext; DBG_INF("QC_TOKEN_DAY_MICROSECOND");	DBG_RETURN(QC_TOKEN_DAY_MICROSECOND); }
(?i:DAY_MINUTE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DAY_MINUTE");		DBG_RETURN(QC_TOKEN_DAY_MINUTE); }
(?i:DAY_SECOND)						{ *kn = yytext; DBG_INF("QC_TOKEN_DAY_SECOND");		DBG_RETURN(QC_TOKEN_DAY_SECOND); }
(?i:DAY)							{ *kn = yytext; DBG_INF("QC_TOKEN_DAY");				DBG_RETURN(QC_TOKEN_DAY); }
(?i:DEALLOCATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DEALLOCATE");		DBG_RETURN(QC_TOKEN_DEALLOCATE); }
(?i:DECIMAL_NUM)					{ *kn = yytext; DBG_INF("QC_TOKEN_DECIMAL_NUM");		DBG_RETURN(QC_TOKEN_DECIMAL_NUM); }
(?i:DECIMAL)						{ *kn = yytext; DBG_INF("QC_TOKEN_DECIMAL");			DBG_RETURN(QC_TOKEN_DECIMAL); }
(?i:DECLARE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DECLARE");			DBG_RETURN(QC_TOKEN_DECLARE); }
(?i:DEFAULT)						{ *kn = yytext; DBG_INF("QC_TOKEN_DEFAULT");			DBG_RETURN(QC_TOKEN_DEFAULT); }
(?i:DEFINER)						{ *kn = yytext; DBG_INF("QC_TOKEN_DEFINER");			DBG_RETURN(QC_TOKEN_DEFINER); }
(?i:DELAYED)						{ *kn = yytext; DBG_INF("QC_TOKEN_DELAYED");			DBG_RETURN(QC_TOKEN_DELAYED); }
(?i:DELAY_KEY_WRITE)				{ *kn = yytext; DBG_INF("QC_TOKEN_DELAY_KEY_WRITE");	DBG_RETURN(QC_TOKEN_DELAY_KEY_WRITE); }
(?i:DELETE)							{ *kn = yytext; DBG_INF("QC_TOKEN_DELETE");			DBG_RETURN(QC_TOKEN_DELETE); }
(?i:DESC)							{ *kn = yytext; DBG_INF("QC_TOKEN_DESC");				DBG_RETURN(QC_TOKEN_DESC); }
(?i:DESCRIBE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DESCRIBE");			DBG_RETURN(QC_TOKEN_DESCRIBE); }
(?i:DES_KEY_FILE)					{ *kn = yytext; DBG_INF("QC_TOKEN_DES_KEY_FILE");		DBG_RETURN(QC_TOKEN_DES_KEY_FILE); }
(?i:DETERMINISTIC)					{ *kn = yytext; DBG_INF("QC_TOKEN_DETERMINISTIC");	DBG_RETURN(QC_TOKEN_DETERMINISTIC); }
(?i:DIRECTORY)						{ *kn = yytext; DBG_INF("QC_TOKEN_DIRECTORY");		DBG_RETURN(QC_TOKEN_DIRECTORY); }
(?i:DISABLE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DISABLE");			DBG_RETURN(QC_TOKEN_DISABLE); }
(?i:DISCARD)						{ *kn = yytext; DBG_INF("QC_TOKEN_DISCARD");			DBG_RETURN(QC_TOKEN_DISCARD); }
(?i:DISK)							{ *kn = yytext; DBG_INF("QC_TOKEN_DISK");				DBG_RETURN(QC_TOKEN_DISK); }
(?i:DISTINCT)						{ *kn = yytext; DBG_INF("QC_TOKEN_DISTINCT");			DBG_RETURN(QC_TOKEN_DISTINCT); }
(?i:DIV)							{ *kn = yytext; DBG_INF("QC_TOKEN_DIV");				DBG_RETURN(QC_TOKEN_DIV); }
(?i:DOUBLE)							{ *kn = yytext; DBG_INF("QC_TOKEN_DOUBLE");			DBG_RETURN(QC_TOKEN_DOUBLE); }
(?i:DO)								{ *kn = yytext; DBG_INF("QC_TOKEN_DO");				DBG_RETURN(QC_TOKEN_DO); }
(?i:DROP)							{ *kn = yytext; DBG_INF("QC_TOKEN_DROP");				DBG_RETURN(QC_TOKEN_DROP); }
(?i:DUAL)							{ *kn = yytext; DBG_INF("QC_TOKEN_DUAL");				DBG_RETURN(QC_TOKEN_DUAL); }
(?i:DUMPFILE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DUMPFILE");			DBG_RETURN(QC_TOKEN_DUMPFILE); }
(?i:DUPLICATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_DUPLICATE");		DBG_RETURN(QC_TOKEN_DUPLICATE); }
(?i:DYNAMIC)						{ *kn = yytext; DBG_INF("QC_TOKEN_DYNAMIC");			DBG_RETURN(QC_TOKEN_DYNAMIC); }
(?i:EACH)							{ *kn = yytext; DBG_INF("QC_TOKEN_EACH");				DBG_RETURN(QC_TOKEN_EACH); }
(?i:ELSE)							{ *kn = yytext; DBG_INF("QC_TOKEN_ELSE");				DBG_RETURN(QC_TOKEN_ELSE); }
(?i:ELSEIF)							{ *kn = yytext; DBG_INF("QC_TOKEN_ELSEIF");			DBG_RETURN(QC_TOKEN_ELSEIF); }
(?i:ENABLE)							{ *kn = yytext; DBG_INF("QC_TOKEN_ENABLE");			DBG_RETURN(QC_TOKEN_ENABLE); }
(?i:ENCLOSED)						{ *kn = yytext; DBG_INF("QC_TOKEN_ENCLOSED");			DBG_RETURN(QC_TOKEN_ENCLOSED); }
(?i:END)							{ *kn = yytext; DBG_INF("QC_TOKEN_END");				DBG_RETURN(QC_TOKEN_END); }
(?i:ENDS)							{ *kn = yytext; DBG_INF("QC_TOKEN_ENDS");				DBG_RETURN(QC_TOKEN_ENDS); }
(?i:ENGINES)						{ *kn = yytext; DBG_INF("QC_TOKEN_ENGINES");			DBG_RETURN(QC_TOKEN_ENGINES); }
(?i:ENGINE)							{ *kn = yytext; DBG_INF("QC_TOKEN_ENGINE");			DBG_RETURN(QC_TOKEN_ENGINE); }
(?i:ENUM)							{ *kn = yytext; DBG_INF("QC_TOKEN_ENUM");				DBG_RETURN(QC_TOKEN_ENUM); }
(?i:EQUAL)							{ *kn = yytext; DBG_INF("QC_TOKEN_EQUAL");			DBG_RETURN(QC_TOKEN_EQUAL); }
(?i:ERRORS)							{ *kn = yytext; DBG_INF("QC_TOKEN_ERRORS");			DBG_RETURN(QC_TOKEN_ERRORS); }
(?i:ESCAPED)						{ *kn = yytext; DBG_INF("QC_TOKEN_ESCAPED");			DBG_RETURN(QC_TOKEN_ESCAPED); }
(?i:ESCAPE)							{ *kn = yytext; DBG_INF("QC_TOKEN_ESCAPE");			DBG_RETURN(QC_TOKEN_ESCAPE); }
(?i:EVENTS)							{ *kn = yytext; DBG_INF("QC_TOKEN_EVENTS");			DBG_RETURN(QC_TOKEN_EVENTS); }
(?i:EVENT)							{ *kn = yytext; DBG_INF("QC_TOKEN_EVENT");			DBG_RETURN(QC_TOKEN_EVENT); }
(?i:EVERY)							{ *kn = yytext; DBG_INF("QC_TOKEN_EVERY");			DBG_RETURN(QC_TOKEN_EVERY); }
(?i:EXECUTE)						{ *kn = yytext; DBG_INF("QC_TOKEN_EXECUTE");			DBG_RETURN(QC_TOKEN_EXECUTE); }
(?i:EXISTS)							{ *kn = yytext; DBG_INF("QC_TOKEN_EXISTS");			DBG_RETURN(QC_TOKEN_EXISTS); }
(?i:EXIT)							{ *kn = yytext; DBG_INF("QC_TOKEN_EXIT");				DBG_RETURN(QC_TOKEN_EXIT); }
(?i:EXPANSION)						{ *kn = yytext; DBG_INF("QC_TOKEN_EXPANSION");		DBG_RETURN(QC_TOKEN_EXPANSION); }
(?i:EXTENDED)						{ *kn = yytext; DBG_INF("QC_TOKEN_EXTENDED");			DBG_RETURN(QC_TOKEN_EXTENDED); }
(?i:EXTENT_SIZE)					{ *kn = yytext; DBG_INF("QC_TOKEN_EXTENT_SIZE");		DBG_RETURN(QC_TOKEN_EXTENT_SIZE); }
(?i:EXTRACT)						{ *kn = yytext; DBG_INF("QC_TOKEN_EXTRACT");			DBG_RETURN(QC_TOKEN_EXTRACT); }
(?i:FALSE)							{ *kn = yytext; DBG_INF("QC_TOKEN_FALSE");			DBG_RETURN(QC_TOKEN_FALSE); }
(?i:FAST)							{ *kn = yytext; DBG_INF("QC_TOKEN_FAST");				DBG_RETURN(QC_TOKEN_FAST); }
(?i:FAULTS)							{ *kn = yytext; DBG_INF("QC_TOKEN_FAULTS");			DBG_RETURN(QC_TOKEN_FAULTS); }
(?i:FETCH)							{ *kn = yytext; DBG_INF("QC_TOKEN_FETCH");			DBG_RETURN(QC_TOKEN_FETCH); }
(?i:FILE)							{ *kn = yytext; DBG_INF("QC_TOKEN_FILE");				DBG_RETURN(QC_TOKEN_FILE); }
(?i:FIRST)							{ *kn = yytext; DBG_INF("QC_TOKEN_FIRST");			DBG_RETURN(QC_TOKEN_FIRST); }
(?i:FIXED)							{ *kn = yytext; DBG_INF("QC_TOKEN_FIXED");			DBG_RETURN(QC_TOKEN_FIXED); }
(?i:FLOAT_NUM)						{ *kn = yytext; DBG_INF("QC_TOKEN_FLOAT_NUM");		DBG_RETURN(QC_TOKEN_FLOAT_NUM); }
(?i:FLOAT)							{ *kn = yytext; DBG_INF("QC_TOKEN_FLOAT");			DBG_RETURN(QC_TOKEN_FLOAT); }
(?i:FLUSH)							{ *kn = yytext; DBG_INF("QC_TOKEN_FLUSH");			DBG_RETURN(QC_TOKEN_FLUSH); }
(?i:FORCE)							{ *kn = yytext; DBG_INF("QC_TOKEN_FORCE");			DBG_RETURN(QC_TOKEN_FORCE); }
(?i:FOREIGN)						{ *kn = yytext; DBG_INF("QC_TOKEN_FOREIGN");			DBG_RETURN(QC_TOKEN_FOREIGN); }
(?i:FOR)							{ *kn = yytext; DBG_INF("QC_TOKEN_FOR");				DBG_RETURN(QC_TOKEN_FOR); }
(?i:FOUND)							{ *kn = yytext; DBG_INF("QC_TOKEN_FOUND");			DBG_RETURN(QC_TOKEN_FOUND); }
(?i:FRAC_SECOND)					{ *kn = yytext; DBG_INF("QC_TOKEN_FRAC_SECOND");		DBG_RETURN(QC_TOKEN_FRAC_SECOND); }
(?i:FROM)							{ *kn = yytext; DBG_INF("QC_TOKEN_FROM");				DBG_RETURN(QC_TOKEN_FROM); }
(?i:FULL)							{ *kn = yytext; DBG_INF("QC_TOKEN_FULL");				DBG_RETURN(QC_TOKEN_FULL); }
(?i:FULLTEXT)						{ *kn = yytext; DBG_INF("QC_TOKEN_FULLTEXT");			DBG_RETURN(QC_TOKEN_FULLTEXT); }
(?i:FUNCTION)						{ *kn = yytext; DBG_INF("QC_TOKEN_FUNCTION");			DBG_RETURN(QC_TOKEN_FUNCTION); }
(?i:GEOMETRYCOLLECTION)				{ *kn = yytext; DBG_INF("QC_TOKEN_GEOMETRYCOLLECTION");DBG_RETURN(QC_TOKEN_GEOMETRYCOLLECTION); }
(?i:GEOMETRY)						{ *kn = yytext; DBG_INF("QC_TOKEN_GEOMETRY");			DBG_RETURN(QC_TOKEN_GEOMETRY); }
(?i:GET_FORMAT)						{ *kn = yytext; DBG_INF("QC_TOKEN_GET_FORMAT");		DBG_RETURN(QC_TOKEN_GET_FORMAT); }
(?i:GLOBAL)							{ *kn = yytext; DBG_INF("QC_TOKEN_GLOBAL");			DBG_RETURN(QC_TOKEN_GLOBAL); }
(?i:GRANT)							{ *kn = yytext; DBG_INF("QC_TOKEN_GRANT");			DBG_RETURN(QC_TOKEN_GRANT); }
(?i:GRANTS)							{ *kn = yytext; DBG_INF("QC_TOKEN_GRANTS");			DBG_RETURN(QC_TOKEN_GRANTS); }
(?i:GROUP[:space]+BY)				{ *kn = yytext; DBG_INF("QC_TOKEN_GROUP");			DBG_RETURN(QC_TOKEN_GROUP); }
(?i:GROUP)							{ *kn = yytext; DBG_INF("QC_TOKEN_GROUP");			DBG_RETURN(QC_TOKEN_GROUP); }
(?i:GROUP_CONCAT)					{ *kn = yytext; DBG_INF("QC_TOKEN_GROUP_CONCAT");		DBG_RETURN(QC_TOKEN_GROUP_CONCAT); }
(?i:HANDLER)						{ *kn = yytext; DBG_INF("QC_TOKEN_HANDLER");			DBG_RETURN(QC_TOKEN_HANDLER); }
(?i:HASH)							{ *kn = yytext; DBG_INF("QC_TOKEN_HASH");				DBG_RETURN(QC_TOKEN_HASH); }
(?i:HAVING)							{ *kn = yytext; DBG_INF("QC_TOKEN_HAVING");			DBG_RETURN(QC_TOKEN_HAVING); }
(?i:HELP)							{ *kn = yytext; DBG_INF("QC_TOKEN_HELP");				DBG_RETURN(QC_TOKEN_HELP); }
(?i:HEX_NUM)						{ *kn = yytext; DBG_INF("QC_TOKEN_HEX_NUM");			DBG_RETURN(QC_TOKEN_HEX_NUM); }
(?i:HIGH_PRIORITY)					{ *kn = yytext; DBG_INF("QC_TOKEN_HIGH_PRIORITY");	DBG_RETURN(QC_TOKEN_HIGH_PRIORITY); }
(?i:HOST)							{ *kn = yytext; DBG_INF("QC_TOKEN_HOST");				DBG_RETURN(QC_TOKEN_HOST); }
(?i:HOSTS)							{ *kn = yytext; DBG_INF("QC_TOKEN_HOSTS");			DBG_RETURN(QC_TOKEN_HOSTS); }
(?i:HOUR_MICROSECOND)				{ *kn = yytext; DBG_INF("QC_TOKEN_HOUR_MICROSECOND");	DBG_RETURN(QC_TOKEN_HOUR_MICROSECOND); }
(?i:HOUR_MINUTE)					{ *kn = yytext; DBG_INF("QC_TOKEN_HOUR_MINUTE");		DBG_RETURN(QC_TOKEN_HOUR_MINUTE); }
(?i:HOUR_SECOND)					{ *kn = yytext; DBG_INF("QC_TOKEN_HOUR_SECOND");		DBG_RETURN(QC_TOKEN_HOUR_SECOND); }
(?i:HOUR)							{ *kn = yytext; DBG_INF("QC_TOKEN_HOUR");				DBG_RETURN(QC_TOKEN_HOUR); }
(?i:IDENT)							{ *kn = yytext; DBG_INF("QC_TOKEN_IDENT");			DBG_RETURN(QC_TOKEN_IDENT); }
(?i:IDENTIFIED)						{ *kn = yytext; DBG_INF("QC_TOKEN_IDENTIFIED");		DBG_RETURN(QC_TOKEN_IDENTIFIED); }
(?i:IDENT_QUOTED)					{ *kn = yytext; DBG_INF("QC_TOKEN_IDENT_QUOTED");		DBG_RETURN(QC_TOKEN_IDENT_QUOTED); }
(?i:IF)								{ *kn = yytext; DBG_INF("QC_TOKEN_IF");				DBG_RETURN(QC_TOKEN_IF); }
(?i:IGNORE)							{ *kn = yytext; DBG_INF("QC_TOKEN_IGNORE");			DBG_RETURN(QC_TOKEN_IGNORE); }
(?i:IGNORE_SERVER_IDS)				{ *kn = yytext; DBG_INF("QC_TOKEN_IGNORE_SERVER_IDS");DBG_RETURN(QC_TOKEN_IGNORE_SERVER_IDS); }
(?i:IMPORT)							{ *kn = yytext; DBG_INF("QC_TOKEN_IMPORT");			DBG_RETURN(QC_TOKEN_IMPORT); }
(?i:INDEXES)						{ *kn = yytext; DBG_INF("QC_TOKEN_INDEXES");			DBG_RETURN(QC_TOKEN_INDEXES); }
(?i:INDEX)							{ *kn = yytext; DBG_INF("QC_TOKEN_INDEX");			DBG_RETURN(QC_TOKEN_INDEX); }
(?i:INFILE)							{ *kn = yytext; DBG_INF("QC_TOKEN_INFILE");			DBG_RETURN(QC_TOKEN_INFILE); }
(?i:INITIAL_SIZE)					{ *kn = yytext; DBG_INF("QC_TOKEN_INITIAL_SIZE");		DBG_RETURN(QC_TOKEN_INITIAL_SIZE); }
(?i:INNER)							{ *kn = yytext; DBG_INF("QC_TOKEN_INNER");			DBG_RETURN(QC_TOKEN_INNER); }
(?i:INOUT)							{ *kn = yytext; DBG_INF("QC_TOKEN_INOUT");			DBG_RETURN(QC_TOKEN_INOUT); }
(?i:INSENSITIVE)					{ *kn = yytext; DBG_INF("QC_TOKEN_INSENSITIVE");		DBG_RETURN(QC_TOKEN_INSENSITIVE); }
(?i:INSERT)							{ *kn = yytext; DBG_INF("QC_TOKEN_INSERT");			DBG_RETURN(QC_TOKEN_INSERT); }
(?i:INSERT_METHOD)					{ *kn = yytext; DBG_INF("QC_TOKEN_INSERT_METHOD");	DBG_RETURN(QC_TOKEN_INSERT_METHOD); }
(?i:INSTALL)						{ *kn = yytext; DBG_INF("QC_TOKEN_INSTALL");			DBG_RETURN(QC_TOKEN_INSTALL); }
(?i:INTERVAL)						{ *kn = yytext; DBG_INF("QC_TOKEN_INTERVAL");			DBG_RETURN(QC_TOKEN_INTERVAL); }
(?i:INTO)							{ *kn = yytext; DBG_INF("QC_TOKEN_INTO");				DBG_RETURN(QC_TOKEN_INTO); }
(?i:INT)							{ *kn = yytext; DBG_INF("QC_TOKEN_INT");				DBG_RETURN(QC_TOKEN_INT); }
(?i:INVOKER)						{ *kn = yytext; DBG_INF("QC_TOKEN_INVOKER");			DBG_RETURN(QC_TOKEN_INVOKER); }
(?i:IN)								{ *kn = yytext; DBG_INF("QC_TOKEN_IN");				DBG_RETURN(QC_TOKEN_IN); }
(?i:IO)								{ *kn = yytext; DBG_INF("QC_TOKEN_IO");				DBG_RETURN(QC_TOKEN_IO); }
(?i:IPC)							{ *kn = yytext; DBG_INF("QC_TOKEN_IPC");				DBG_RETURN(QC_TOKEN_IPC); }
(?i:IS)								{ *kn = yytext; DBG_INF("QC_TOKEN_IS");				DBG_RETURN(QC_TOKEN_IS); }
(?i:ISOLATION)						{ *kn = yytext; DBG_INF("QC_TOKEN_ISOLATION");		DBG_RETURN(QC_TOKEN_ISOLATION); }
(?i:ISSUER)							{ *kn = yytext; DBG_INF("QC_TOKEN_ISSUER");			DBG_RETURN(QC_TOKEN_ISSUER); }
(?i:ITERATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_ITERATE");			DBG_RETURN(QC_TOKEN_ITERATE); }
(?i:JOIN)							{ *kn = yytext; DBG_INF("QC_TOKEN_JOIN");				DBG_RETURN(QC_TOKEN_JOIN); }
(?i:KEYS)							{ *kn = yytext; DBG_INF("QC_TOKEN_KEYS");				DBG_RETURN(QC_TOKEN_KEYS); }
(?i:KEY_BLOCK_SIZE)					{ *kn = yytext; DBG_INF("QC_TOKEN_KEY_BLOCK_SIZE");	DBG_RETURN(QC_TOKEN_KEY_BLOCK_SIZE); }
(?i:KEY)							{ *kn = yytext; DBG_INF("QC_TOKEN_KEY");				DBG_RETURN(QC_TOKEN_KEY); }
(?i:KILL)							{ *kn = yytext; DBG_INF("QC_TOKEN_KILL");				DBG_RETURN(QC_TOKEN_KILL); }
(?i:LANGUAGE)						{ *kn = yytext; DBG_INF("QC_TOKEN_LANGUAGE");			DBG_RETURN(QC_TOKEN_LANGUAGE); }
(?i:LAST)							{ *kn = yytext; DBG_INF("QC_TOKEN_LAST");				DBG_RETURN(QC_TOKEN_LAST); }
(?i:LEADING)						{ *kn = yytext; DBG_INF("QC_TOKEN_LEADING");			DBG_RETURN(QC_TOKEN_LEADING); }
(?i:LEAVES)							{ *kn = yytext; DBG_INF("QC_TOKEN_LEAVES");			DBG_RETURN(QC_TOKEN_LEAVES); }
(?i:LEAVE)							{ *kn = yytext; DBG_INF("QC_TOKEN_LEAVE");			DBG_RETURN(QC_TOKEN_LEAVE); }
(?i:LEFT)							{ *kn = yytext; DBG_INF("QC_TOKEN_LEFT");				DBG_RETURN(QC_TOKEN_LEFT); }
(?i:LESS)							{ *kn = yytext; DBG_INF("QC_TOKEN_LESS");				DBG_RETURN(QC_TOKEN_LESS); }
(?i:LEVEL)							{ *kn = yytext; DBG_INF("QC_TOKEN_LEVEL");			DBG_RETURN(QC_TOKEN_LEVEL); }
(?i:LEX_HOSTNAME)					{ *kn = yytext; DBG_INF("QC_TOKEN_LEX_HOSTNAME");		DBG_RETURN(QC_TOKEN_LEX_HOSTNAME); }
(?i:LIKE)							{ *kn = yytext; DBG_INF("QC_TOKEN_LIKE");				DBG_RETURN(QC_TOKEN_LIKE); }
(?i:LIMIT)							{ *kn = yytext; DBG_INF("QC_TOKEN_LIMIT");			DBG_RETURN(QC_TOKEN_LIMIT); }
(?i:LINEAR)							{ *kn = yytext; DBG_INF("QC_TOKEN_LINEAR");			DBG_RETURN(QC_TOKEN_LINEAR); }
(?i:LINES)							{ *kn = yytext; DBG_INF("QC_TOKEN_LINES");			DBG_RETURN(QC_TOKEN_LINES); }
(?i:LINESTRING)						{ *kn = yytext; DBG_INF("QC_TOKEN_LINESTRING");		DBG_RETURN(QC_TOKEN_LINESTRING); }
(?i:LIST)							{ *kn = yytext; DBG_INF("QC_TOKEN_LIST");				DBG_RETURN(QC_TOKEN_LIST); }
(?i:LOAD)							{ *kn = yytext; DBG_INF("QC_TOKEN_LOAD");				DBG_RETURN(QC_TOKEN_LOAD); }
(?i:LOCAL)							{ *kn = yytext; DBG_INF("QC_TOKEN_LOCAL");			DBG_RETURN(QC_TOKEN_LOCAL); }
(?i:LOCATOR)						{ *kn = yytext; DBG_INF("QC_TOKEN_LOCATOR");			DBG_RETURN(QC_TOKEN_LOCATOR); }
(?i:LOCKS)							{ *kn = yytext; DBG_INF("QC_TOKEN_LOCKS");			DBG_RETURN(QC_TOKEN_LOCKS); }
(?i:LOCK)							{ *kn = yytext; DBG_INF("QC_TOKEN_LOCK");				DBG_RETURN(QC_TOKEN_LOCK); }
(?i:LOGFILE)						{ *kn = yytext; DBG_INF("QC_TOKEN_LOGFILE");			DBG_RETURN(QC_TOKEN_LOGFILE); }
(?i:LOGS)							{ *kn = yytext; DBG_INF("QC_TOKEN_LOGS");				DBG_RETURN(QC_TOKEN_LOGS); }
(?i:LONGBLOB)						{ *kn = yytext; DBG_INF("QC_TOKEN_LONGBLOB");			DBG_RETURN(QC_TOKEN_LONGBLOB); }
(?i:LONGTEXT)						{ *kn = yytext; DBG_INF("QC_TOKEN_LONGTEXT");			DBG_RETURN(QC_TOKEN_LONGTEXT); }
(?i:LONG_NUM)						{ *kn = yytext; DBG_INF("QC_TOKEN_LONG_NUM");			DBG_RETURN(QC_TOKEN_LONG_NUM); }
(?i:LONG)							{ *kn = yytext; DBG_INF("QC_TOKEN_LONG");				DBG_RETURN(QC_TOKEN_LONG); }
(?i:LOOP)							{ *kn = yytext; DBG_INF("QC_TOKEN_LOOP");				DBG_RETURN(QC_TOKEN_LOOP); }
(?i:LOW_PRIORITY)					{ *kn = yytext; DBG_INF("QC_TOKEN_LOW_PRIORITY");		DBG_RETURN(QC_TOKEN_LOW_PRIORITY); }
(?i:MASTER_CONNECT_RETRY)			{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_CONNECT_RETRY");			DBG_RETURN(QC_TOKEN_MASTER_CONNECT_RETRY); }
(?i:MASTER_HOST)					{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_HOST");		DBG_RETURN(QC_TOKEN_MASTER_HOST); }
(?i:MASTER_LOG_FILE)				{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_LOG_FILE");	DBG_RETURN(QC_TOKEN_MASTER_LOG_FILE); }
(?i:MASTER_LOG_POS)					{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_LOG_POS");	DBG_RETURN(QC_TOKEN_MASTER_LOG_POS); }
(?i:MASTER_PASSWORD)				{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_PASSWORD");	DBG_RETURN(QC_TOKEN_MASTER_PASSWORD); }
(?i:MASTER_PORT)					{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_PORT");		DBG_RETURN(QC_TOKEN_MASTER_PORT); }
(?i:MASTER_SERVER_ID)				{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_SERVER_ID");	DBG_RETURN(QC_TOKEN_MASTER_SERVER_ID); }
(?i:MASTER_SSL_CAPATH)				{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_SSL_CAPATH");DBG_RETURN(QC_TOKEN_MASTER_SSL_CAPATH); }
(?i:MASTER_SSL_CA)					{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_SSL_CA");	DBG_RETURN(QC_TOKEN_MASTER_SSL_CA); }
(?i:MASTER_SSL_CERT)				{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_SSL_CERT");	DBG_RETURN(QC_TOKEN_MASTER_SSL_CERT); }
(?i:MASTER_SSL_CIPHER)				{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_SSL_CIPHER");DBG_RETURN(QC_TOKEN_MASTER_SSL_CIPHER); }
(?i:MASTER_SSL_KEY)					{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_SSL_KEY");	DBG_RETURN(QC_TOKEN_MASTER_SSL_KEY); }
(?i:MASTER_SSL)						{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_SSL");		DBG_RETURN(QC_TOKEN_MASTER_SSL); }
(?i:MASTER_SSL_VERIFY_SERVER_CERT)	{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_SSL_VERIFY_SERVER_CERT");			DBG_RETURN(QC_TOKEN_MASTER_SSL_VERIFY_SERVER_CERT); }
(?i:MASTER)							{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER");			DBG_RETURN(QC_TOKEN_MASTER); }
(?i:MASTER_USER)					{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_USER");		DBG_RETURN(QC_TOKEN_MASTER_USER); }
(?i:MASTER_HEARTBEAT_PERIOD)		{ *kn = yytext; DBG_INF("QC_TOKEN_MASTER_HEARTBEAT_PERIOD");			DBG_RETURN(QC_TOKEN_MASTER_HEARTBEAT_PERIOD); }
(?i:MATCH)							{ *kn = yytext; DBG_INF("QC_TOKEN_MATCH");			DBG_RETURN(QC_TOKEN_MATCH); }
(?i:MAX_CONNECTIONS_PER_HOUR)		{ *kn = yytext; DBG_INF("QC_TOKEN_MAX_CONNECTIONS_PER_HOUR");			DBG_RETURN(QC_TOKEN_MAX_CONNECTIONS_PER_HOUR); }
(?i:MAX_QUERIES_PER_HOUR)			{ *kn = yytext; DBG_INF("QC_TOKEN_MAX_QUERIES_PER_HOUR");			DBG_RETURN(QC_TOKEN_MAX_QUERIES_PER_HOUR); }
(?i:MAX_ROWS)						{ *kn = yytext; DBG_INF("QC_TOKEN_MAX_ROWS");			DBG_RETURN(QC_TOKEN_MAX_ROWS); }
(?i:MAX_SIZE)						{ *kn = yytext; DBG_INF("QC_TOKEN_MAX_SIZE");			DBG_RETURN(QC_TOKEN_MAX_SIZE); }
(?i:MAX)							{ *kn = yytext; DBG_INF("QC_TOKEN_MAX");			DBG_RETURN(QC_TOKEN_MAX); }
(?i:MAX_UPDATES_PER_HOUR)			{ *kn = yytext; DBG_INF("QC_TOKEN_MAX_UPDATES_PER_HOUR");			DBG_RETURN(QC_TOKEN_MAX_UPDATES_PER_HOUR); }
(?i:MAX_USER_CONNECTIONS)			{ *kn = yytext; DBG_INF("QC_TOKEN_MAX_USER_CONNECTIONS");			DBG_RETURN(QC_TOKEN_MAX_USER_CONNECTIONS); }
(?i:MAX_VALUE)						{ *kn = yytext; DBG_INF("QC_TOKEN_MAX_VALUE");		DBG_RETURN(QC_TOKEN_MAX_VALUE); }
(?i:MEDIUMBLOB)						{ *kn = yytext; DBG_INF("QC_TOKEN_MEDIUMBLOB");		DBG_RETURN(QC_TOKEN_MEDIUMBLOB); }
(?i:MEDIUMINT)						{ *kn = yytext; DBG_INF("QC_TOKEN_MEDIUMINT");		DBG_RETURN(QC_TOKEN_MEDIUMINT); }
(?i:MEDIUMTEXT)						{ *kn = yytext; DBG_INF("QC_TOKEN_MEDIUMTEXT");		DBG_RETURN(QC_TOKEN_MEDIUMTEXT); }
(?i:MEDIUM)							{ *kn = yytext; DBG_INF("QC_TOKEN_MEDIUM");			DBG_RETURN(QC_TOKEN_MEDIUM); }
(?i:MEMORY)							{ *kn = yytext; DBG_INF("QC_TOKEN_MEMORY");			DBG_RETURN(QC_TOKEN_MEMORY); }
(?i:MERGE)							{ *kn = yytext; DBG_INF("QC_TOKEN_MERGE");			DBG_RETURN(QC_TOKEN_MERGE); }
(?i:MESSAGE_TEXT)					{ *kn = yytext; DBG_INF("QC_TOKEN_MESSAGE_TEXT");		DBG_RETURN(QC_TOKEN_MESSAGE_TEXT); }
(?i:MICROSECOND)					{ *kn = yytext; DBG_INF("QC_TOKEN_MICROSECOND");		DBG_RETURN(QC_TOKEN_MICROSECOND); }
(?i:MIGRATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_MIGRATE");			DBG_RETURN(QC_TOKEN_MIGRATE); }
(?i:MINUTE_MICROSECOND)				{ *kn = yytext; DBG_INF("QC_TOKEN_MINUTE_MICROSECOND");DBG_RETURN(QC_TOKEN_MINUTE_MICROSECOND); }
(?i:MINUTE_SECOND)					{ *kn = yytext; DBG_INF("QC_TOKEN_MINUTE_SECOND");	DBG_RETURN(QC_TOKEN_MINUTE_SECOND); }
(?i:MINUTE)							{ *kn = yytext; DBG_INF("QC_TOKEN_MINUTE");			DBG_RETURN(QC_TOKEN_MINUTE); }
(?i:MIN_ROWS)						{ *kn = yytext; DBG_INF("QC_TOKEN_MIN_ROWS");			DBG_RETURN(QC_TOKEN_MIN_ROWS); }
(?i:MIN)							{ *kn = yytext; DBG_INF("QC_TOKEN_MIN");				DBG_RETURN(QC_TOKEN_MIN); }
(?i:MODE)							{ *kn = yytext; DBG_INF("QC_TOKEN_MODE");				DBG_RETURN(QC_TOKEN_MODE); }
(?i:MODIFIES)						{ *kn = yytext; DBG_INF("QC_TOKEN_MODIFIES");			DBG_RETURN(QC_TOKEN_MODIFIES); }
(?i:MODIFY)							{ *kn = yytext; DBG_INF("QC_TOKEN_MODIFY");			DBG_RETURN(QC_TOKEN_MODIFY); }
(?i:MOD)							{ *kn = yytext; DBG_INF("QC_TOKEN_MOD");				DBG_RETURN(QC_TOKEN_MOD); }
(?i:MONTH)							{ *kn = yytext; DBG_INF("QC_TOKEN_MONTH");			DBG_RETURN(QC_TOKEN_MONTH); }
(?i:MULTILINESTRING)				{ *kn = yytext; DBG_INF("QC_TOKEN_MULTILINESTRING");	DBG_RETURN(QC_TOKEN_MULTILINESTRING); }
(?i:MULTIPOINT)						{ *kn = yytext; DBG_INF("QC_TOKEN_MULTIPOINT");		DBG_RETURN(QC_TOKEN_MULTIPOINT); }
(?i:MULTIPOLYGON)					{ *kn = yytext; DBG_INF("QC_TOKEN_MULTIPOLYGON");		DBG_RETURN(QC_TOKEN_MULTIPOLYGON); }
(?i:MUTEX)							{ *kn = yytext; DBG_INF("QC_TOKEN_MUTEX");			DBG_RETURN(QC_TOKEN_MUTEX); }
(?i:MYSQL_ERRNO)					{ *kn = yytext; DBG_INF("QC_TOKEN_MYSQL_ERRNO");		DBG_RETURN(QC_TOKEN_MYSQL_ERRNO); }
(?i:NAMES)							{ *kn = yytext; DBG_INF("QC_TOKEN_NAMES");			DBG_RETURN(QC_TOKEN_NAMES); }
(?i:NAME)							{ *kn = yytext; DBG_INF("QC_TOKEN_NAME");				DBG_RETURN(QC_TOKEN_NAME); }
(?i:NATIONAL)						{ *kn = yytext; DBG_INF("QC_TOKEN_NATIONAL");			DBG_RETURN(QC_TOKEN_NATIONAL); }
(?i:NATURAL)						{ *kn = yytext; DBG_INF("QC_TOKEN_NATURAL");			DBG_RETURN(QC_TOKEN_NATURAL); }
(?i:NCHAR_STRING)					{ *kn = yytext; DBG_INF("QC_TOKEN_NCHAR_STRING");		DBG_RETURN(QC_TOKEN_NCHAR_STRING); }
(?i:NCHAR)							{ *kn = yytext; DBG_INF("QC_TOKEN_NCHAR");			DBG_RETURN(QC_TOKEN_NCHAR); }
(?i:NDBCLUSTER)						{ *kn = yytext; DBG_INF("QC_TOKEN_NDBCLUSTER");		DBG_RETURN(QC_TOKEN_NDBCLUSTER); }
(?i:NEG)							{ *kn = yytext; DBG_INF("QC_TOKEN_NEG");				DBG_RETURN(QC_TOKEN_NEG); }
(?i:NEW)							{ *kn = yytext; DBG_INF("QC_TOKEN_NEW");				DBG_RETURN(QC_TOKEN_NEW); }
(?i:NEXT)							{ *kn = yytext; DBG_INF("QC_TOKEN_NEXT");				DBG_RETURN(QC_TOKEN_NEXT); }
(?i:NODEGROUP)						{ *kn = yytext; DBG_INF("QC_TOKEN_NODEGROUP");		DBG_RETURN(QC_TOKEN_NODEGROUP); }
(?i:NONE)							{ *kn = yytext; DBG_INF("QC_TOKEN_NONE");				DBG_RETURN(QC_TOKEN_NONE); }
(?i:NOT)							{ *kn = yytext; DBG_INF("QC_TOKEN_NOT");				DBG_RETURN(QC_TOKEN_NOT); }
(?i:NOW)							{ *kn = yytext; DBG_INF("QC_TOKEN_NOW");				DBG_RETURN(QC_TOKEN_NOW); }
(?i:NO)								{ *kn = yytext; DBG_INF("QC_TOKEN_NO");				DBG_RETURN(QC_TOKEN_NO); }
(?i:NO_WAIT)						{ *kn = yytext; DBG_INF("QC_TOKEN_NO_WAIT");			DBG_RETURN(QC_TOKEN_NO_WAIT); }
(?i:NO_WRITE_TO_BINLOG)				{ *kn = yytext; DBG_INF("QC_TOKEN_NO_WRITE_TO_BINLOG");DBG_RETURN(QC_TOKEN_NO_WRITE_TO_BINLOG); }
(?i:NULL)							{ *kn = yytext; DBG_INF("QC_TOKEN_NULL");				DBG_RETURN(QC_TOKEN_NULL); }
(?i:NUM)							{ *kn = yytext; DBG_INF("QC_TOKEN_NUM");				DBG_RETURN(QC_TOKEN_NUM); }
(?i:NUMERIC)						{ *kn = yytext; DBG_INF("QC_TOKEN_NUMERIC");			DBG_RETURN(QC_TOKEN_NUMERIC); }
(?i:NVARCHAR)						{ *kn = yytext; DBG_INF("QC_TOKEN_NVARCHAR");			DBG_RETURN(QC_TOKEN_NVARCHAR); }
(?i:OFFSET)							{ *kn = yytext; DBG_INF("QC_TOKEN_OFFSET");			DBG_RETURN(QC_TOKEN_OFFSET); }
(?i:OLD_PASSWORD)					{ *kn = yytext; DBG_INF("QC_TOKEN_OLD_PASSWORD");		DBG_RETURN(QC_TOKEN_OLD_PASSWORD); }
(?i:ON)								{ *kn = yytext; DBG_INF("QC_TOKEN_ON");				DBG_RETURN(QC_TOKEN_ON); }
(?i:ONE_SHOT)						{ *kn = yytext; DBG_INF("QC_TOKEN_ONE_SHOT");			DBG_RETURN(QC_TOKEN_ONE_SHOT); }
(?i:ONE)							{ *kn = yytext; DBG_INF("QC_TOKEN_ONE");				DBG_RETURN(QC_TOKEN_ONE); }
(?i:OPEN)							{ *kn = yytext; DBG_INF("QC_TOKEN_OPEN");				DBG_RETURN(QC_TOKEN_OPEN); }
(?i:OPTIMIZE)						{ *kn = yytext; DBG_INF("QC_TOKEN_OPTIMIZE");			DBG_RETURN(QC_TOKEN_OPTIMIZE); }
(?i:OPTIONS)						{ *kn = yytext; DBG_INF("QC_TOKEN_OPTIONS");			DBG_RETURN(QC_TOKEN_OPTIONS); }
(?i:OPTION)							{ *kn = yytext; DBG_INF("QC_TOKEN_OPTION");			DBG_RETURN(QC_TOKEN_OPTION); }
(?i:OPTIONALLY)						{ *kn = yytext; DBG_INF("QC_TOKEN_OPTIONALLY");		DBG_RETURN(QC_TOKEN_OPTIONALLY); }
(?i:ORDER)							{ *kn = yytext; DBG_INF("QC_TOKEN_ORDER");			DBG_RETURN(QC_TOKEN_ORDER); }
(?i:OR)								{ *kn = yytext; DBG_INF("QC_TOKEN_OR");				DBG_RETURN(QC_TOKEN_OR); }
(?i:OUTER)							{ *kn = yytext; DBG_INF("QC_TOKEN_OUTER");			DBG_RETURN(QC_TOKEN_OUTER); }
(?i:OUTFILE)						{ *kn = yytext; DBG_INF("QC_TOKEN_OUTFILE");			DBG_RETURN(QC_TOKEN_OUTFILE); }
(?i:OUT)							{ *kn = yytext; DBG_INF("QC_TOKEN_OUT");				DBG_RETURN(QC_TOKEN_OUT); }
(?i:OWNER)							{ *kn = yytext; DBG_INF("QC_TOKEN_OWNER");			DBG_RETURN(QC_TOKEN_OWNER); }
(?i:PACK_KEYS)						{ *kn = yytext; DBG_INF("QC_TOKEN_PACK_KEYS");		DBG_RETURN(QC_TOKEN_PACK_KEYS); }
(?i:PAGE)							{ *kn = yytext; DBG_INF("QC_TOKEN_PAGE");				DBG_RETURN(QC_TOKEN_PAGE); }
(?i:PARAM_MARKER)					{ *kn = yytext; DBG_INF("QC_TOKEN_PARAM_MARKER");		DBG_RETURN(QC_TOKEN_PARAM_MARKER); }
(?i:PARSER)							{ *kn = yytext; DBG_INF("QC_TOKEN_PARSER");			DBG_RETURN(QC_TOKEN_PARSER); }
(?i:PARTIAL)						{ *kn = yytext; DBG_INF("QC_TOKEN_PARTIAL");			DBG_RETURN(QC_TOKEN_PARTIAL); }
(?i:PARTITIONING)					{ *kn = yytext; DBG_INF("QC_TOKEN_PARTITIONING");		DBG_RETURN(QC_TOKEN_PARTITIONING); }
(?i:PARTITIONS)						{ *kn = yytext; DBG_INF("QC_TOKEN_PARTITIONS");		DBG_RETURN(QC_TOKEN_PARTITIONS); }
(?i:PARTITION)						{ *kn = yytext; DBG_INF("QC_TOKEN_PARTITION");		DBG_RETURN(QC_TOKEN_PARTITION); }
(?i:PASSWORD)						{ *kn = yytext; DBG_INF("QC_TOKEN_PASSWORD");			DBG_RETURN(QC_TOKEN_PASSWORD); }
(?i:PHASE)							{ *kn = yytext; DBG_INF("QC_TOKEN_PHASE");			DBG_RETURN(QC_TOKEN_PHASE); }
(?i:PLUGINS)						{ *kn = yytext; DBG_INF("QC_TOKEN_PLUGINS");			DBG_RETURN(QC_TOKEN_PLUGINS); }
(?i:PLUGIN)							{ *kn = yytext; DBG_INF("QC_TOKEN_PLUGIN");			DBG_RETURN(QC_TOKEN_PLUGIN); }
(?i:POINT)							{ *kn = yytext; DBG_INF("QC_TOKEN_POINT");			DBG_RETURN(QC_TOKEN_POINT); }
(?i:POLYGON)						{ *kn = yytext; DBG_INF("QC_TOKEN_POLYGON");			DBG_RETURN(QC_TOKEN_POLYGON); }
(?i:PORT)							{ *kn = yytext; DBG_INF("QC_TOKEN_PORT");				DBG_RETURN(QC_TOKEN_PORT); }
(?i:POSITION)						{ *kn = yytext; DBG_INF("QC_TOKEN_POSITION");			DBG_RETURN(QC_TOKEN_POSITION); }
(?i:PRECISION)						{ *kn = yytext; DBG_INF("QC_TOKEN_PRECISION");		DBG_RETURN(QC_TOKEN_PRECISION); }
(?i:PREPARE)						{ *kn = yytext; DBG_INF("QC_TOKEN_PREPARE");			DBG_RETURN(QC_TOKEN_PREPARE); }
(?i:PRESERVE)						{ *kn = yytext; DBG_INF("QC_TOKEN_PRESERVE");			DBG_RETURN(QC_TOKEN_PRESERVE); }
(?i:PREV)							{ *kn = yytext; DBG_INF("QC_TOKEN_PREV");				DBG_RETURN(QC_TOKEN_PREV); }
(?i:PRIMARY)						{ *kn = yytext; DBG_INF("QC_TOKEN_PRIMARY");			DBG_RETURN(QC_TOKEN_PRIMARY); }
(?i:PRIVILEGES)						{ *kn = yytext; DBG_INF("QC_TOKEN_PRIVILEGES");		DBG_RETURN(QC_TOKEN_PRIVILEGES); }
(?i:PROCEDURE)						{ *kn = yytext; DBG_INF("QC_TOKEN_PROCEDURE");		DBG_RETURN(QC_TOKEN_PROCEDURE); }
(?i:PROCESS)						{ *kn = yytext; DBG_INF("QC_TOKEN_PROCESS");			DBG_RETURN(QC_TOKEN_PROCESS); }
(?i:PROCESSLIST)					{ *kn = yytext; DBG_INF("QC_TOKEN_PROCESSLIST");		DBG_RETURN(QC_TOKEN_PROCESSLIST); }
(?i:PROFILE)						{ *kn = yytext; DBG_INF("QC_TOKEN_PROFILE");			DBG_RETURN(QC_TOKEN_PROFILE); }
(?i:PROFILES)						{ *kn = yytext; DBG_INF("QC_TOKEN_PROFILES");			DBG_RETURN(QC_TOKEN_PROFILES); }
(?i:PURGE)							{ *kn = yytext; DBG_INF("QC_TOKEN_PURGE");			DBG_RETURN(QC_TOKEN_PURGE); }
(?i:QUARTER)						{ *kn = yytext; DBG_INF("QC_TOKEN_QUARTER");			DBG_RETURN(QC_TOKEN_QUARTER); }
(?i:QUERY)							{ *kn = yytext; DBG_INF("QC_TOKEN_QUERY");			DBG_RETURN(QC_TOKEN_QUERY); }
(?i:QUICK)							{ *kn = yytext; DBG_INF("QC_TOKEN_QUICK");			DBG_RETURN(QC_TOKEN_QUICK); }
(?i:RANGE)							{ *kn = yytext; DBG_INF("QC_TOKEN_RANGE");			DBG_RETURN(QC_TOKEN_RANGE); }
(?i:READS)							{ *kn = yytext; DBG_INF("QC_TOKEN_READS");			DBG_RETURN(QC_TOKEN_READS); }
(?i:READ_ONLY)						{ *kn = yytext; DBG_INF("QC_TOKEN_READ_ONLY");		DBG_RETURN(QC_TOKEN_READ_ONLY); }
(?i:READ)							{ *kn = yytext; DBG_INF("QC_TOKEN_READ");				DBG_RETURN(QC_TOKEN_READ); }
(?i:READ_WRITE)						{ *kn = yytext; DBG_INF("QC_TOKEN_READ_WRITE");		DBG_RETURN(QC_TOKEN_READ_WRITE); }
(?i:REAL)							{ *kn = yytext; DBG_INF("QC_TOKEN_REAL");				DBG_RETURN(QC_TOKEN_REAL); }
(?i:REBUILD)						{ *kn = yytext; DBG_INF("QC_TOKEN_REBUILD");			DBG_RETURN(QC_TOKEN_REBUILD); }
(?i:RECOVER)						{ *kn = yytext; DBG_INF("QC_TOKEN_RECOVER");			DBG_RETURN(QC_TOKEN_RECOVER); }
(?i:REDOFILE)						{ *kn = yytext; DBG_INF("QC_TOKEN_REDOFILE");			DBG_RETURN(QC_TOKEN_REDOFILE); }
(?i:REDO_BUFFER_SIZE)				{ *kn = yytext; DBG_INF("QC_TOKEN_REDO_BUFFER_SIZE");	DBG_RETURN(QC_TOKEN_REDO_BUFFER_SIZE); }
(?i:REDUNDANT)						{ *kn = yytext; DBG_INF("QC_TOKEN_REDUNDANT");		DBG_RETURN(QC_TOKEN_REDUNDANT); }
(?i:REFERENCES)						{ *kn = yytext; DBG_INF("QC_TOKEN_REFERENCES");		DBG_RETURN(QC_TOKEN_REFERENCES); }
(?i:REGEXP)							{ *kn = yytext; DBG_INF("QC_TOKEN_REGEXP");			DBG_RETURN(QC_TOKEN_REGEXP); }
(?i:RELAYLOG)						{ *kn = yytext; DBG_INF("QC_TOKEN_RELAYLOG");			DBG_RETURN(QC_TOKEN_RELAYLOG); }
(?i:RELAY_LOG_FILE)					{ *kn = yytext; DBG_INF("QC_TOKEN_RELAY_LOG_FILE");	DBG_RETURN(QC_TOKEN_RELAY_LOG_FILE); }
(?i:RELAY_LOG_POS)					{ *kn = yytext; DBG_INF("QC_TOKEN_RELAY_LOG_POS");	DBG_RETURN(QC_TOKEN_RELAY_LOG_POS); }
(?i:RELAY_THREAD)					{ *kn = yytext; DBG_INF("QC_TOKEN_RELAY_THREAD");		DBG_RETURN(QC_TOKEN_RELAY_THREAD); }
(?i:RELEASE)						{ *kn = yytext; DBG_INF("QC_TOKEN_RELEASE");			DBG_RETURN(QC_TOKEN_RELEASE); }
(?i:RELOAD)							{ *kn = yytext; DBG_INF("QC_TOKEN_RELOAD");			DBG_RETURN(QC_TOKEN_RELOAD); }
(?i:REMOVE)							{ *kn = yytext; DBG_INF("QC_TOKEN_REMOVE");			DBG_RETURN(QC_TOKEN_REMOVE); }
(?i:RENAME)							{ *kn = yytext; DBG_INF("QC_TOKEN_RENAME");			DBG_RETURN(QC_TOKEN_RENAME); }
(?i:REORGANIZE)						{ *kn = yytext; DBG_INF("QC_TOKEN_REORGANIZE");		DBG_RETURN(QC_TOKEN_REORGANIZE); }
(?i:REPAIR)							{ *kn = yytext; DBG_INF("QC_TOKEN_REPAIR");			DBG_RETURN(QC_TOKEN_REPAIR); }
(?i:REPEATABLE)						{ *kn = yytext; DBG_INF("QC_TOKEN_REPEATABLE");		DBG_RETURN(QC_TOKEN_REPEATABLE); }
(?i:REPEAT)							{ *kn = yytext; DBG_INF("QC_TOKEN_REPEAT");			DBG_RETURN(QC_TOKEN_REPEAT); }
(?i:REPLACE)						{ *kn = yytext; DBG_INF("QC_TOKEN_REPLACE");			DBG_RETURN(QC_TOKEN_REPLACE); }
(?i:REPLICATION)					{ *kn = yytext; DBG_INF("QC_TOKEN_REPLICATION");		DBG_RETURN(QC_TOKEN_REPLICATION); }
(?i:REQUIRE)						{ *kn = yytext; DBG_INF("QC_TOKEN_REQUIRE");			DBG_RETURN(QC_TOKEN_REQUIRE); }
(?i:RESET)							{ *kn = yytext; DBG_INF("QC_TOKEN_RESET");			DBG_RETURN(QC_TOKEN_RESET); }
(?i:RESIGNAL)						{ *kn = yytext; DBG_INF("QC_TOKEN_RESIGNAL");			DBG_RETURN(QC_TOKEN_RESIGNAL); }
(?i:RESOURCES)						{ *kn = yytext; DBG_INF("QC_TOKEN_RESOURCES");		DBG_RETURN(QC_TOKEN_RESOURCES); }
(?i:RESTORE)						{ *kn = yytext; DBG_INF("QC_TOKEN_RESTORE");			DBG_RETURN(QC_TOKEN_RESTORE); }
(?i:RESTRICT)						{ *kn = yytext; DBG_INF("QC_TOKEN_RESTRICT");			DBG_RETURN(QC_TOKEN_RESTRICT); }
(?i:RESUME)							{ *kn = yytext; DBG_INF("QC_TOKEN_RESUME");			DBG_RETURN(QC_TOKEN_RESUME); }
(?i:RETURNS)						{ *kn = yytext; DBG_INF("QC_TOKEN_RETURNS");			DBG_RETURN(QC_TOKEN_RETURNS); }
(?i:RETURN)							{ *kn = yytext; DBG_INF("QC_TOKEN_RETURN");			DBG_RETURN(QC_TOKEN_RETURN); }
(?i:REVOKE)							{ *kn = yytext; DBG_INF("QC_TOKEN_REVOKE");			DBG_RETURN(QC_TOKEN_REVOKE); }
(?i:RIGHT)							{ *kn = yytext; DBG_INF("QC_TOKEN_RIGHT");			DBG_RETURN(QC_TOKEN_RIGHT); }
(?i:ROLLBACK)						{ *kn = yytext; DBG_INF("QC_TOKEN_ROLLBACK");			DBG_RETURN(QC_TOKEN_ROLLBACK); }
(?i:ROLLUP)							{ *kn = yytext; DBG_INF("QC_TOKEN_ROLLUP");			DBG_RETURN(QC_TOKEN_ROLLUP); }
(?i:ROUTINE)						{ *kn = yytext; DBG_INF("QC_TOKEN_ROUTINE");			DBG_RETURN(QC_TOKEN_ROUTINE); }
(?i:ROWS)							{ *kn = yytext; DBG_INF("QC_TOKEN_ROWS");				DBG_RETURN(QC_TOKEN_ROWS); }
(?i:ROW_FORMAT)						{ *kn = yytext; DBG_INF("QC_TOKEN_ROW_FORMAT");		DBG_RETURN(QC_TOKEN_ROW_FORMAT); }
(?i:ROW)							{ *kn = yytext; DBG_INF("QC_TOKEN_ROW");				DBG_RETURN(QC_TOKEN_ROW); }
(?i:RTREE)							{ *kn = yytext; DBG_INF("QC_TOKEN_RTREE");			DBG_RETURN(QC_TOKEN_RTREE); }
(?i:SAVEPOINT)						{ *kn = yytext; DBG_INF("QC_TOKEN_SAVEPOINT");		DBG_RETURN(QC_TOKEN_SAVEPOINT); }
(?i:SCHEDULE)						{ *kn = yytext; DBG_INF("QC_TOKEN_SCHEDULE");			DBG_RETURN(QC_TOKEN_SCHEDULE); }
(?i:SCHEMA_NAME)					{ *kn = yytext; DBG_INF("QC_TOKEN_SCHEMA_NAME");		DBG_RETURN(QC_TOKEN_SCHEMA_NAME); }
(?i:SECOND_MICROSECOND)				{ *kn = yytext; DBG_INF("QC_TOKEN_SECOND_MICROSECOND");			DBG_RETURN(QC_TOKEN_SECOND_MICROSECOND); }
(?i:SECOND)							{ *kn = yytext; DBG_INF("QC_TOKEN_SECOND");			DBG_RETURN(QC_TOKEN_SECOND); }
(?i:SECURITY)						{ *kn = yytext; DBG_INF("QC_TOKEN_SECURITY");			DBG_RETURN(QC_TOKEN_SECURITY); }
(?i:SELECT)							{ *kn = yytext; DBG_INF("QC_TOKEN_SELECT");			DBG_RETURN(QC_TOKEN_SELECT); }
(?i:SENSITIVE)						{ *kn = yytext; DBG_INF("QC_TOKEN_SENSITIVE");		DBG_RETURN(QC_TOKEN_SENSITIVE); }
(?i:SEPARATOR)						{ *kn = yytext; DBG_INF("QC_TOKEN_SEPARATOR");		DBG_RETURN(QC_TOKEN_SEPARATOR); }
(?i:SERIALIZABLE)					{ *kn = yytext; DBG_INF("QC_TOKEN_SERIALIZABLE");		DBG_RETURN(QC_TOKEN_SERIALIZABLE); }
(?i:SERIAL)							{ *kn = yytext; DBG_INF("QC_TOKEN_SERIAL");			DBG_RETURN(QC_TOKEN_SERIAL); }
(?i:SESSION)						{ *kn = yytext; DBG_INF("QC_TOKEN_SESSION");			DBG_RETURN(QC_TOKEN_SESSION); }
(?i:SERVER)							{ *kn = yytext; DBG_INF("QC_TOKEN_SERVER");			DBG_RETURN(QC_TOKEN_SERVER); }
(?i:SERVER_OPTIONS)					{ *kn = yytext; DBG_INF("QC_TOKEN_SERVER_OPTIONS");	DBG_RETURN(QC_TOKEN_SERVER_OPTIONS); }
(?i:SET)							{ *kn = yytext; DBG_INF("QC_TOKEN_SET");				DBG_RETURN(QC_TOKEN_SET); }
(?i:SHARE)							{ *kn = yytext; DBG_INF("QC_TOKEN_SHARE");			DBG_RETURN(QC_TOKEN_SHARE); }
(?i:SHIFT_LEFT)						{ *kn = yytext; DBG_INF("QC_TOKEN_SHIFT_LEFT");		DBG_RETURN(QC_TOKEN_SHIFT_LEFT); }
(?i:SHIFT_RIGHT)					{ *kn = yytext; DBG_INF("QC_TOKEN_SHIFT_RIGHT");		DBG_RETURN(QC_TOKEN_SHIFT_RIGHT); }
(?i:SHOW)							{ *kn = yytext; DBG_INF("QC_TOKEN_SHOW");				DBG_RETURN(QC_TOKEN_SHOW); }
(?i:SHUTDOWN)						{ *kn = yytext; DBG_INF("QC_TOKEN_SHUTDOWN");			DBG_RETURN(QC_TOKEN_SHUTDOWN); }
(?i:SIGNAL)							{ *kn = yytext; DBG_INF("QC_TOKEN_SIGNAL");			DBG_RETURN(QC_TOKEN_SIGNAL); }
(?i:SIGNED)							{ *kn = yytext; DBG_INF("QC_TOKEN_SIGNED");			DBG_RETURN(QC_TOKEN_SIGNED); }
(?i:SIMPLE)							{ *kn = yytext; DBG_INF("QC_TOKEN_SIMPLE");			DBG_RETURN(QC_TOKEN_SIMPLE); }
(?i:SLAVE)							{ *kn = yytext; DBG_INF("QC_TOKEN_SLAVE");			DBG_RETURN(QC_TOKEN_SLAVE); }
(?i:SMALLINT)						{ *kn = yytext; DBG_INF("QC_TOKEN_SMALLINT");			DBG_RETURN(QC_TOKEN_SMALLINT); }
(?i:SNAPSHOT)						{ *kn = yytext; DBG_INF("QC_TOKEN_SNAPSHOT");			DBG_RETURN(QC_TOKEN_SNAPSHOT); }
(?i:SOCKET)							{ *kn = yytext; DBG_INF("QC_TOKEN_SOCKET");			DBG_RETURN(QC_TOKEN_SOCKET); }
(?i:SONAME)							{ *kn = yytext; DBG_INF("QC_TOKEN_SONAME");			DBG_RETURN(QC_TOKEN_SONAME); }
(?i:SOUNDS)							{ *kn = yytext; DBG_INF("QC_TOKEN_SOUNDS");			DBG_RETURN(QC_TOKEN_SOUNDS); }
(?i:SOURCE)							{ *kn = yytext; DBG_INF("QC_TOKEN_SOURCE");			DBG_RETURN(QC_TOKEN_SOURCE); }
(?i:SPATIAL)						{ *kn = yytext; DBG_INF("QC_TOKEN_SPATIAL");			DBG_RETURN(QC_TOKEN_SPATIAL); }
(?i:SPECIFIC)						{ *kn = yytext; DBG_INF("QC_TOKEN_SPECIFIC");			DBG_RETURN(QC_TOKEN_SPECIFIC); }
(?i:SQLEXCEPTION)					{ *kn = yytext; DBG_INF("QC_TOKEN_SQLEXCEPTION");		DBG_RETURN(QC_TOKEN_SQLEXCEPTION); }
(?i:SQLSTATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_SQLSTATE");			DBG_RETURN(QC_TOKEN_SQLSTATE); }
(?i:SQLWARNING)						{ *kn = yytext; DBG_INF("QC_TOKEN_SQLWARNING");		DBG_RETURN(QC_TOKEN_SQLWARNING); }
(?i:SQL_BIG_RESULT)					{ *kn = yytext; DBG_INF("QC_TOKEN_SQL_BIG_RESULT");	DBG_RETURN(QC_TOKEN_SQL_BIG_RESULT); }
(?i:SQL_BUFFER_RESULT)				{ *kn = yytext; DBG_INF("QC_TOKEN_SQL_BUFFER_RESULT");DBG_RETURN(QC_TOKEN_SQL_BUFFER_RESULT); }
(?i:SQL_CACHE)						{ *kn = yytext; DBG_INF("QC_TOKEN_SQL_CACHE");		DBG_RETURN(QC_TOKEN_SQL_CACHE); }
(?i:SQL_CALC_FOUND_ROWS)			{ *kn = yytext; DBG_INF("QC_TOKEN_SQL_CALC_FOUND_ROWS");			DBG_RETURN(QC_TOKEN_SQL_CALC_FOUND_ROWS); }
(?i:SQL_NO_CACHE)					{ *kn = yytext; DBG_INF("QC_TOKEN_SQL_NO_CACHE");		DBG_RETURN(QC_TOKEN_SQL_NO_CACHE); }
(?i:SQL_SMALL_RESULT)				{ *kn = yytext; DBG_INF("QC_TOKEN_SQL_SMALL_RESULT");	DBG_RETURN(QC_TOKEN_SQL_SMALL_RESULT); }
(?i:SQL)							{ *kn = yytext; DBG_INF("QC_TOKEN_SQL");			DBG_RETURN(QC_TOKEN_SQL); }
(?i:SQL_THREAD)						{ *kn = yytext; DBG_INF("QC_TOKEN_SQL_THREAD");	DBG_RETURN(QC_TOKEN_SQL_THREAD); }
(?i:SSL)							{ *kn = yytext; DBG_INF("QC_TOKEN_SSL");			DBG_RETURN(QC_TOKEN_SSL); }
(?i:STARTING)						{ *kn = yytext; DBG_INF("QC_TOKEN_STARTING");		DBG_RETURN(QC_TOKEN_STARTING); }
(?i:STARTS)							{ *kn = yytext; DBG_INF("QC_TOKEN_STARTS");		DBG_RETURN(QC_TOKEN_STARTS); }
(?i:START)							{ *kn = yytext; DBG_INF("QC_TOKEN_START");		DBG_RETURN(QC_TOKEN_START); }
(?i:STATUS)							{ *kn = yytext; DBG_INF("QC_TOKEN_STATUS");		DBG_RETURN(QC_TOKEN_STATUS); }
(?i:STDDEV_SAMP)					{ *kn = yytext; DBG_INF("QC_TOKEN_STDDEV_SAMP");	DBG_RETURN(QC_TOKEN_STDDEV_SAMP); }
(?i:STD)							{ *kn = yytext; DBG_INF("QC_TOKEN_STD");			DBG_RETURN(QC_TOKEN_STD); }
(?i:STOP)							{ *kn = yytext; DBG_INF("QC_TOKEN_STOP");			DBG_RETURN(QC_TOKEN_STOP); }
(?i:STORAGE)						{ *kn = yytext; DBG_INF("QC_TOKEN_STORAGE");		DBG_RETURN(QC_TOKEN_STORAGE); }
(?i:STRAIGHT[:space]+JOIN)			{ *kn = yytext; DBG_INF("QC_TOKEN_STRAIGHT_JOIN");DBG_RETURN(QC_TOKEN_STRAIGHT_JOIN); }
(?i:STRING)							{ *kn = yytext; DBG_INF("QC_TOKEN_STRING");		DBG_RETURN(QC_TOKEN_STRING); }
(?i:SUBCLASS_ORIGIN)				{ *kn = yytext; DBG_INF("QC_TOKEN_SUBCLASS_ORIGIN");DBG_RETURN(QC_TOKEN_SUBCLASS_ORIGIN); }
(?i:SUBDATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_SUBDATE");		DBG_RETURN(QC_TOKEN_SUBDATE); }
(?i:SUBJECT)						{ *kn = yytext; DBG_INF("QC_TOKEN_SUBJECT");		DBG_RETURN(QC_TOKEN_SUBJECT); }
(?i:SUBPARTITIONS)					{ *kn = yytext; DBG_INF("QC_TOKEN_SUBPARTITIONS");DBG_RETURN(QC_TOKEN_SUBPARTITIONS); }
(?i:SUBPARTITION)					{ *kn = yytext; DBG_INF("QC_TOKEN_SUBPARTITION");	DBG_RETURN(QC_TOKEN_SUBPARTITION); }
(?i:SUBSTRING)						{ *kn = yytext; DBG_INF("QC_TOKEN_SUBSTRING");	DBG_RETURN(QC_TOKEN_SUBSTRING); }
(?i:SUM)							{ *kn = yytext; DBG_INF("QC_TOKEN_SUM");			DBG_RETURN(QC_TOKEN_SUM); }
(?i:SUPER)							{ *kn = yytext; DBG_INF("QC_TOKEN_SUPER");		DBG_RETURN(QC_TOKEN_SUPER); }
(?i:SUSPEND)						{ *kn = yytext; DBG_INF("QC_TOKEN_SUSPEND");		DBG_RETURN(QC_TOKEN_SUSPEND); }
(?i:SWAPS)							{ *kn = yytext; DBG_INF("QC_TOKEN_SWAPS");		DBG_RETURN(QC_TOKEN_SWAPS); }
(?i:SWITCHES)						{ *kn = yytext; DBG_INF("QC_TOKEN_SWITCHES");		DBG_RETURN(QC_TOKEN_SWITCHES); }
(?i:SYSDATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_SYSDATE");		DBG_RETURN(QC_TOKEN_SYSDATE); }
(?i:TABLES)							{ *kn = yytext; DBG_INF("QC_TOKEN_TABLES");		DBG_RETURN(QC_TOKEN_TABLES); }
(?i:TABLESPACE)						{ *kn = yytext; DBG_INF("QC_TOKEN_TABLESPACE");	DBG_RETURN(QC_TOKEN_TABLESPACE); }
(?i:TABLE_REF_PRIORITY)				{ *kn = yytext; DBG_INF("QC_TOKEN_TABLE_REF_PRIORITY");DBG_RETURN(QC_TOKEN_TABLE_REF_PRIORITY); }
(?i:TABLE)							{ *kn = yytext; DBG_INF("QC_TOKEN_TABLE");		DBG_RETURN(QC_TOKEN_TABLE); }
(?i:TABLE_CHECKSUM)					{ *kn = yytext; DBG_INF("QC_TOKEN_TABLE_CHECKSUM");DBG_RETURN(QC_TOKEN_TABLE_CHECKSUM); }
(?i:TABLE_NAME)						{ *kn = yytext; DBG_INF("QC_TOKEN_TABLE_NAME");	DBG_RETURN(QC_TOKEN_TABLE_NAME); }
(?i:TEMPORARY)						{ *kn = yytext; DBG_INF("QC_TOKEN_TEMPORARY");	DBG_RETURN(QC_TOKEN_TEMPORARY); }
(?i:TEMPTABLE)						{ *kn = yytext; DBG_INF("QC_TOKEN_TEMPTABLE");	DBG_RETURN(QC_TOKEN_TEMPTABLE); }
(?i:TERMINATED)						{ *kn = yytext; DBG_INF("QC_TOKEN_TERMINATED");	DBG_RETURN(QC_TOKEN_TERMINATED); }
(?i:TEXT_STRING)					{ *kn = yytext; DBG_INF("QC_TOKEN_TEXT_STRING");	DBG_RETURN(QC_TOKEN_TEXT_STRING); }
(?i:TEXT)							{ *kn = yytext; DBG_INF("QC_TOKEN_TEXT");			DBG_RETURN(QC_TOKEN_TEXT); }
(?i:THAN)							{ *kn = yytext; DBG_INF("QC_TOKEN_THAN");			DBG_RETURN(QC_TOKEN_THAN); }
(?i:THEN)							{ *kn = yytext; DBG_INF("QC_TOKEN_THEN");			DBG_RETURN(QC_TOKEN_THEN); }
(?i:TIMESTAMP)						{ *kn = yytext; DBG_INF("QC_TOKEN_TIMESTAMP");	DBG_RETURN(QC_TOKEN_TIMESTAMP); }
(?i:TIMESTAMP_ADD)					{ *kn = yytext; DBG_INF("QC_TOKEN_TIMESTAMP_ADD");DBG_RETURN(QC_TOKEN_TIMESTAMP_ADD); }
(?i:TIMESTAMP_DIFF)					{ *kn = yytext; DBG_INF("QC_TOKEN_TIMESTAMP_DIFF");DBG_RETURN(QC_TOKEN_TIMESTAMP_DIFF); }
(?i:TIME)							{ *kn = yytext; DBG_INF("QC_TOKEN_TIME");			DBG_RETURN(QC_TOKEN_TIME); }
(?i:TINYBLOB)						{ *kn = yytext; DBG_INF("QC_TOKEN_TINYBLOB");		DBG_RETURN(QC_TOKEN_TINYBLOB); }
(?i:TINYINT)						{ *kn = yytext; DBG_INF("QC_TOKEN_TINYINT");		DBG_RETURN(QC_TOKEN_TINYINT); }
(?i:TINYTEXT)						{ *kn = yytext; DBG_INF("QC_TOKEN_TINYTEXT");		DBG_RETURN(QC_TOKEN_TINYTEXT); }
(?i:TO)								{ *kn = yytext; DBG_INF("QC_TOKEN_TO");			DBG_RETURN(QC_TOKEN_TO); }
(?i:TRAILING)						{ *kn = yytext; DBG_INF("QC_TOKEN_TRAILING");		DBG_RETURN(QC_TOKEN_TRAILING); }
(?i:TRANSACTION)					{ *kn = yytext; DBG_INF("QC_TOKEN_TRANSACTION");	DBG_RETURN(QC_TOKEN_TRANSACTION); }
(?i:TRIGGERS)						{ *kn = yytext; DBG_INF("QC_TOKEN_TRIGGERS");		DBG_RETURN(QC_TOKEN_TRIGGERS); }
(?i:TRIGGER)						{ *kn = yytext; DBG_INF("QC_TOKEN_TRIGGER");		DBG_RETURN(QC_TOKEN_TRIGGER); }
(?i:TRIM)							{ *kn = yytext; DBG_INF("QC_TOKEN_TRIM");			DBG_RETURN(QC_TOKEN_TRIM); }
(?i:TRUE)							{ *kn = yytext; DBG_INF("QC_TOKEN_TRUE");			DBG_RETURN(QC_TOKEN_TRUE); }
(?i:TRUNCATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_TRUNCATE");		DBG_RETURN(QC_TOKEN_TRUNCATE); }
(?i:TYPES)							{ *kn = yytext; DBG_INF("QC_TOKEN_TYPES");		DBG_RETURN(QC_TOKEN_TYPES); }
(?i:TYPE)							{ *kn = yytext; DBG_INF("QC_TOKEN_TYPE");			DBG_RETURN(QC_TOKEN_TYPE); }
(?i:UDF_RETURNS)					{ *kn = yytext; DBG_INF("QC_TOKEN_UDF_RETURNS");	DBG_RETURN(QC_TOKEN_UDF_RETURNS); }
(?i:ULONGLONG_NUM)					{ *kn = yytext; DBG_INF("QC_TOKEN_ULONGLONG_NUM");DBG_RETURN(QC_TOKEN_ULONGLONG_NUM); }
(?i:UNCOMMITTED)					{ *kn = yytext; DBG_INF("QC_TOKEN_UNCOMMITTED");	DBG_RETURN(QC_TOKEN_UNCOMMITTED); }
(?i:UNDEFINED)						{ *kn = yytext; DBG_INF("QC_TOKEN_UNDEFINED");	DBG_RETURN(QC_TOKEN_UNDEFINED); }
(?i:UNDERSCORE_CHARSET)				{ *kn = yytext; DBG_INF("QC_TOKEN_UNDERSCORE_CHARSET");DBG_RETURN(QC_TOKEN_UNDERSCORE_CHARSET); }
(?i:UNDOFILE)						{ *kn = yytext; DBG_INF("QC_TOKEN_UNDOFILE");		DBG_RETURN(QC_TOKEN_UNDOFILE); }
(?i:UNDO_BUFFER_SIZE)				{ *kn = yytext; DBG_INF("QC_TOKEN_UNDO_BUFFER_SIZE");DBG_RETURN(QC_TOKEN_UNDO_BUFFER_SIZE); }
(?i:UNDO)							{ *kn = yytext; DBG_INF("QC_TOKEN_UNDO");			DBG_RETURN(QC_TOKEN_UNDO); }
(?i:UNICODE)						{ *kn = yytext; DBG_INF("QC_TOKEN_UNICODE");		DBG_RETURN(QC_TOKEN_UNICODE); }
(?i:UNINSTALL)						{ *kn = yytext; DBG_INF("QC_TOKEN_UNINSTALL");	DBG_RETURN(QC_TOKEN_UNINSTALL); }
(?i:UNION)							{ *kn = yytext; DBG_INF("QC_TOKEN_UNION");		DBG_RETURN(QC_TOKEN_UNION); }
(?i:UNIQUE)							{ *kn = yytext; DBG_INF("QC_TOKEN_UNIQUE");		DBG_RETURN(QC_TOKEN_UNIQUE); }
(?i:UNKNOWN)						{ *kn = yytext; DBG_INF("QC_TOKEN_UNKNOWN");		DBG_RETURN(QC_TOKEN_UNKNOWN); }
(?i:UNLOCK)							{ *kn = yytext; DBG_INF("QC_TOKEN_UNLOCK");		DBG_RETURN(QC_TOKEN_UNLOCK); }
(?i:UNSIGNED)						{ *kn = yytext; DBG_INF("QC_TOKEN_UNSIGNED");		DBG_RETURN(QC_TOKEN_UNSIGNED); }
(?i:UNTIL)							{ *kn = yytext; DBG_INF("QC_TOKEN_UNTIL");		DBG_RETURN(QC_TOKEN_UNTIL); }
(?i:UPDATE)							{ *kn = yytext; DBG_INF("QC_TOKEN_UPDATE");		DBG_RETURN(QC_TOKEN_UPDATE); }
(?i:UPGRADE)						{ *kn = yytext; DBG_INF("QC_TOKEN_UPGRADE");		DBG_RETURN(QC_TOKEN_UPGRADE); }
(?i:USAGE)							{ *kn = yytext; DBG_INF("QC_TOKEN_USAGE");		DBG_RETURN(QC_TOKEN_USAGE); }
(?i:USER)							{ *kn = yytext; DBG_INF("QC_TOKEN_USER");			DBG_RETURN(QC_TOKEN_USER); }
(?i:USE_FRM)						{ *kn = yytext; DBG_INF("QC_TOKEN_USE_FRM");		DBG_RETURN(QC_TOKEN_USE_FRM); }
(?i:USE)							{ *kn = yytext; DBG_INF("QC_TOKEN_USE");			DBG_RETURN(QC_TOKEN_USE); }
(?i:USING)							{ *kn = yytext; DBG_INF("QC_TOKEN_USING");		DBG_RETURN(QC_TOKEN_USING); }
(?i:UTC_DATE)						{ *kn = yytext; DBG_INF("QC_TOKEN_UTC_DATE");		DBG_RETURN(QC_TOKEN_UTC_DATE); }
(?i:UTC_TIMESTAMP)					{ *kn = yytext; DBG_INF("QC_TOKEN_UTC_TIMESTAMP");DBG_RETURN(QC_TOKEN_UTC_TIMESTAMP); }
(?i:UTC_TIME)						{ *kn = yytext; DBG_INF("QC_TOKEN_UTC_TIME");		DBG_RETURN(QC_TOKEN_UTC_TIME); }
(?i:VALUES)							{ *kn = yytext; DBG_INF("QC_TOKEN_VALUES");		DBG_RETURN(QC_TOKEN_VALUES); }
(?i:VALUE)							{ *kn = yytext; DBG_INF("QC_TOKEN_VALUE");		DBG_RETURN(QC_TOKEN_VALUE); }
(?i:VARBINARY)						{ *kn = yytext; DBG_INF("QC_TOKEN_VARBINARY");	DBG_RETURN(QC_TOKEN_VARBINARY); }
(?i:VARCHAR)						{ *kn = yytext; DBG_INF("QC_TOKEN_VARCHAR");		DBG_RETURN(QC_TOKEN_VARCHAR); }
(?i:VARIABLES)						{ *kn = yytext; DBG_INF("QC_TOKEN_VARIABLES");	DBG_RETURN(QC_TOKEN_VARIABLES); }
(?i:VARIANCE)						{ *kn = yytext; DBG_INF("QC_TOKEN_VARIANCE");		DBG_RETURN(QC_TOKEN_VARIANCE); }
(?i:VARYING)						{ *kn = yytext; DBG_INF("QC_TOKEN_VARYING");		DBG_RETURN(QC_TOKEN_VARYING); }
(?i:VAR_SAMP)						{ *kn = yytext; DBG_INF("QC_TOKEN_VAR_SAMP");		DBG_RETURN(QC_TOKEN_VAR_SAMP); }
(?i:VIEW)							{ *kn = yytext; DBG_INF("QC_TOKEN_VIEW");			DBG_RETURN(QC_TOKEN_VIEW); }
(?i:WAIT)							{ *kn = yytext; DBG_INF("QC_TOKEN_WAIT");			DBG_RETURN(QC_TOKEN_WAIT); }
(?i:WARNINGS)						{ *kn = yytext; DBG_INF("QC_TOKEN_WARNINGS");		DBG_RETURN(QC_TOKEN_WARNINGS); }
(?i:WEEK)							{ *kn = yytext; DBG_INF("QC_TOKEN_WEEK");			DBG_RETURN(QC_TOKEN_WEEK); }
(?i:WHEN)							{ *kn = yytext; DBG_INF("QC_TOKEN_WHEN");			DBG_RETURN(QC_TOKEN_WHEN); }
(?i:WHERE)							{ *kn = yytext; DBG_INF("QC_TOKEN_WHERE");		DBG_RETURN(QC_TOKEN_WHERE); }
(?i:WHILE)							{ *kn = yytext; DBG_INF("QC_TOKEN_WHILE");		DBG_RETURN(QC_TOKEN_WHILE); }
(?i:WITH)							{ *kn = yytext; DBG_INF("QC_TOKEN_WITH");			DBG_RETURN(QC_TOKEN_WITH); }
(?i:WITH[:spacee]+CUBE)				{ *kn = yytext; DBG_INF("QC_TOKEN_WITH_CUBE");	DBG_RETURN(QC_TOKEN_WITH_CUBE); }
(?i:WITH[:spacee]+ROLLUP)			{ *kn = yytext; DBG_INF("QC_TOKEN_WITH_ROLLUP");	DBG_RETURN(QC_TOKEN_WITH_ROLLUP); }
(?i:WORK)							{ *kn = yytext; DBG_INF("QC_TOKEN_WORK");			DBG_RETURN(QC_TOKEN_WORK); }
(?i:WRAPPER)						{ *kn = yytext; DBG_INF("QC_TOKEN_WRAPPER");		DBG_RETURN(QC_TOKEN_WRAPPER); }
(?i:WRITE)							{ *kn = yytext; DBG_INF("QC_TOKEN_WRITE");		DBG_RETURN(QC_TOKEN_WRITE); }
(?i:X509)							{ *kn = yytext; DBG_INF("QC_TOKEN_X509");			DBG_RETURN(QC_TOKEN_X509); }
(?i:XA)								{ *kn = yytext; DBG_INF("QC_TOKEN_XA");			DBG_RETURN(QC_TOKEN_XA); }
(?i:XML)							{ *kn = yytext; DBG_INF("QC_TOKEN_XML");			DBG_RETURN(QC_TOKEN_XML); }
(?i:XOR)							{ *kn = yytext; DBG_INF("QC_TOKEN_XOR");			DBG_RETURN(QC_TOKEN_XOR); }
(?i:YEAR_MONTH)						{ *kn = yytext; DBG_INF("QC_TOKEN_YEAR_MONTH");	DBG_RETURN(QC_TOKEN_YEAR_MONTH); }
(?i:YEAR)							{ *kn = yytext; DBG_INF("QC_TOKEN_YEAR");			DBG_RETURN(QC_TOKEN_YEAR); }
(?i:ZEROFILL)						{ *kn = yytext; DBG_INF("QC_TOKEN_ZEROFILL");		DBG_RETURN(QC_TOKEN_ZEROFILL); }
(?i:CLIENT_FLAG)					{ *kn = yytext; DBG_INF("QC_TOKEN_CLIENT_FLAG");	DBG_RETURN(QC_TOKEN_CLIENT_FLAG); }


	/* Integers and Floats */
-?[0-9]+						{ ZVAL_LONG(token_value, atoi(yytext)); DBG_INF("QC_TOKEN_INTNUM");	DBG_RETURN(QC_TOKEN_INTNUM); }

-?[0-9]+"."[0-9]* |
-?"."[0-9]+ |
-?[0-9]+E[-+]?[0-9]+ |
-?[0-9]+"."[0-9]*E[-+]?[0-9]+ |
-?"."[0-9]+E[-+]?[0-9]+			{ ZVAL_DOUBLE(token_value, atof(yytext)); DBG_INF("QC_TOKEN_FLOATNUM");	DBG_RETURN(QC_TOKEN_FLOATNUM); }

	/* Normal strings */
'(\\.|''|[^'\n])*' |
\"(\\.|\"\"|[^"\n])*\"			{ ZVAL_STRINGL(token_value, yytext, yyleng, 1); DBG_INF("QC_TOKEN_STRING");	DBG_RETURN(QC_TOKEN_STRING); }
'(\\.|[^'\n])*$					{ yyerror("Unterminated string %s", yytext); }
\"(\\.|[^"\n])*$				{ yyerror("Unterminated string %s", yytext); }

	/* Hex and Bit strings */
X'[0-9A-F]+' |
0X[0-9A-F]+						{ ZVAL_STRINGL(token_value, yytext, yyleng, 1); DBG_INF("QC_TOKEN_STRING");	DBG_RETURN(QC_TOKEN_STRING); }
0B[01]+ |
B'[01]+'						{ ZVAL_STRINGL(token_value, yytext, yyleng, 1); DBG_INF("QC_TOKEN_STRING");	DBG_RETURN(QC_TOKEN_STRING); }

	/* Operators */
\+								{ DBG_INF("QC_TOKEN_PLUS");			DBG_RETURN(QC_TOKEN_PLUS); }
\-								{ DBG_INF("QC_TOKEN_MINUS");		DBG_RETURN(QC_TOKEN_MINUS); }
\,								{ DBG_INF("QC_TOKEN_COMMA");		DBG_RETURN(QC_TOKEN_COMMA); }
\;								{ DBG_INF("QC_TOKEN_SEMICOLON");	DBG_RETURN(QC_TOKEN_SEMICOLON); }
\(								{ DBG_INF("QC_TOKEN_BRACKET_OPEN");	DBG_RETURN(QC_TOKEN_BRACKET_OPEN); }
\)								{ DBG_INF("QC_TOKEN_BRACKET_CLOSE");DBG_RETURN(QC_TOKEN_BRACKET_CLOSE); }
\*								{ DBG_INF("QC_TOKEN_STAR");			DBG_RETURN(QC_TOKEN_STAR); }
\.								{ DBG_INF("QC_TOKEN_DOT");			DBG_RETURN(QC_TOKEN_DOT); }
!								{ DBG_INF("QC_TOKEN_NOT");			DBG_RETURN(QC_TOKEN_NOT); }
\^								{ DBG_INF("QC_TOKEN_XOR");			DBG_RETURN(QC_TOKEN_XOR); }
\%								{ DBG_INF("QC_TOKEN_MOD");			DBG_RETURN(QC_TOKEN_MOD); }
\/								{ DBG_INF("QC_TOKEN_DIV");			DBG_RETURN(QC_TOKEN_DIV); }
\~								{ DBG_INF("QC_TOKEN_TILDE");		DBG_RETURN(QC_TOKEN_TILDE); }
"@@"							{ DBG_INF("QC_TOKEN_GLOBAL_VAR");	DBG_RETURN(QC_TOKEN_GLOBAL_VAR); }
\@								{ DBG_INF("QC_TOKEN_SESSION_VAR");	DBG_RETURN(QC_TOKEN_SESSION_VAR); }
"&&"							{ DBG_INF("QC_TOKEN_AND");			DBG_RETURN(QC_TOKEN_AND); }
\&								{ DBG_INF("QC_TOKEN_BIT_AND");		DBG_RETURN(QC_TOKEN_BIT_AND); }
"||"							{ DBG_INF("QC_TOKEN_OR");			DBG_RETURN(QC_TOKEN_OR); }
\|								{ DBG_INF("QC_TOKEN_BIT_OR");		DBG_RETURN(QC_TOKEN_BIT_OR); }
"="								{ DBG_INF("QC_TOKEN_EQ");			DBG_RETURN(QC_TOKEN_EQ); }
"<=>"							{ DBG_INF("QC_TOKEN_NE_TRIPLE");	DBG_RETURN(QC_TOKEN_NE_TRIPLE); }
">="							{ DBG_INF("QC_TOKEN_GE");			DBG_RETURN(QC_TOKEN_GE); }
">"								{ DBG_INF("QC_TOKEN_GT");			DBG_RETURN(QC_TOKEN_GT); }
"<="							{ DBG_INF("QC_TOKEN_LE");			DBG_RETURN(QC_TOKEN_LE); }
"<"								{ DBG_INF("QC_TOKEN_LT");			DBG_RETURN(QC_TOKEN_LT); }
"!=" | "<>"						{ DBG_INF("QC_TOKEN_NE");			DBG_RETURN(QC_TOKEN_NE); }
"<<"							{ DBG_INF("QC_TOKEN_SHIFT_LEFT");	DBG_RETURN(QC_TOKEN_SHIFT_LEFT); }
">>"							{ DBG_INF("QC_TOKEN_SHIFT_RIGHT");	DBG_RETURN(QC_TOKEN_SHIFT_RIGHT); }
":="							{ DBG_INF("QC_TOKEN_ASSIGN_TO_VAR");DBG_RETURN(QC_TOKEN_ASSIGN_TO_VAR); }

	/* normal identifier */
([A-Za-z$_]|[\xC2-\xDF][\x80-\xBF]|\xE0[\xA0-\xBF][\x80-\xBF]|[\xE1-\xEF][\x80-\xBF][\x80-\xBF])([0-9]|[A-Za-z$_]|[\xC2-\xDF][\x80-\xBF]|\xE0[\xA0-\xBF][\x80-\xBF]|[\xE1-\xEF][\x80-\xBF][\x80-\xBF])* { ZVAL_STRINGL(token_value, yytext, yyleng, 1); DBG_INF("QC_TOKEN_IDENTIFIER"); DBG_RETURN(QC_TOKEN_IDENTIFIER); }

	/* quoted identifier */
`[^`/\\.\n]+`					{
									ZVAL_STRINGL(token_value, yytext + 1, yyleng - 2, 1);
									DBG_INF("QC_TOKEN_IDENTIFIER");
									DBG_RETURN(QC_TOKEN_IDENTIFIER);
								}

	/* Comments */
#.* ;							{
									ZVAL_STRINGL(token_value, yytext + 1, yyleng - 1, 1);
									DBG_INF("QC_TOKEN_COMMENT");
									DBG_RETURN(QC_TOKEN_COMMENT);
								}

"--"[ \t].*						{
									ZVAL_STRINGL(token_value, yytext + 2, yyleng - 2, 1);
									DBG_INF("QC_TOKEN_COMMENT");
									DBG_RETURN(QC_TOKEN_COMMENT);
								}

"/*" 							{
									old_yystate = YY_START;
									DBG_INF("entering COMMENT_MODE");
									BEGIN COMMENT_MODE;
#if VALGRIND_MEMORY_WARNINGS
									if (*comment) {
										mnd_efree(*comment);
										*comment = NULL;
									}
#endif
									*comment = mnd_ecalloc(1, sizeof(smart_str));

									ZVAL_NULL(token_value);
								}

<COMMENT_MODE>"*/" 				{
									BEGIN old_yystate;
									DBG_INF("leaving COMMENT_MODE");
									DBG_INF("QC_TOKEN_COMMENT");

									smart_str_appendc(*comment, '\0');
									/*
									  we need to copy the smart_str by value before we set token_value
									  because comment and token_value are the vary same thing (part of an union)
									  if we write something to token_value we will lose comment;
									*/
									{
										smart_str * ss_copy = *comment;
										ZVAL_STRINGL(token_value, (*comment)->c, (*comment)->len, 1);

										smart_str_free(ss_copy);
										mnd_efree(ss_copy);
									}
									DBG_INF_FMT("token_value is now:%s", Z_STRVAL_P(token_value));

									DBG_RETURN(QC_TOKEN_COMMENT);
								}

<COMMENT_MODE>.|\n 				{
									smart_str_appendc(*comment, yytext[0]);
								}

	/* the rest */
[ \t\n] /* whitespace */
. 								{ yyerror("report to the developer '%c'\n", *yytext); }
%%


/* {{{ mysqlnd_qp_error */
int
mysqlnd_qp_error(const char *format, ...)
{
	/* do not emit a message */
	return 1;
}
/* }}} */


/* {{{ mysqlnd_qp_free_scanner */
PHP_MYSQLND_MS_API void
mysqlnd_qp_free_scanner(struct st_mysqlnd_query_scanner * scanner TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_qp_free_scanner");
	if (scanner) {
		yylex_destroy(*(yyscan_t *) scanner->scanner);
		mnd_efree(scanner->scanner);

		mnd_efree(scanner);
	}

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_qp_create_scanner */
PHP_MYSQLND_MS_API struct st_mysqlnd_query_scanner *
mysqlnd_qp_create_scanner(TSRMLS_D)
{
	struct st_mysqlnd_query_scanner * ret = mnd_ecalloc(1, sizeof(struct st_mysqlnd_query_scanner));

	DBG_ENTER("mysqlnd_qp_create_scanner");

	ret->scanner = mnd_ecalloc(1, sizeof(yyscan_t));

	if (yylex_init_extra(ret->token_value /* yyextra */, (yyscan_t *) ret->scanner)) {
		DBG_ERR_FMT("yylex_init_extra failed");
		mysqlnd_qp_free_scanner(ret TSRMLS_CC);
		ret = NULL;
	}
	DBG_INF_FMT("ret=%p", ret);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_qp_get_token */
PHP_MYSQLND_MS_API struct st_ms_token_and_value
mysqlnd_qp_get_token(struct st_mysqlnd_query_scanner * scanner TSRMLS_DC)
{
	YYSTYPE lex_val;
	struct st_ms_token_and_value ret = {0};

	DBG_ENTER("mysqlnd_qp_get_token");

	memset(&lex_val, 0, sizeof(lex_val));
	INIT_ZVAL(lex_val.zv);
	/* yylex expects `yyscan_t`, not `yyscan_t*` */
	if ((ret.token = yylex(&lex_val, *(yyscan_t *)scanner->scanner TSRMLS_CC))) {
		DBG_INF_FMT("token=%d", ret.token);
		switch (Z_TYPE(lex_val.zv)) {
			case IS_STRING:
				DBG_INF_FMT("strval=%s", Z_STRVAL(lex_val.zv));
				ret.value = lex_val.zv;
				break;
			case IS_LONG:
				DBG_INF_FMT("lval=%ld", Z_LVAL(lex_val.zv));
				ret.value = lex_val.zv;
				break;
			case IS_DOUBLE:
				DBG_INF_FMT("dval=%f", Z_DVAL(lex_val.zv));
				ret.value = lex_val.zv;
				break;
			case IS_NULL:
				if (lex_val.kn) {
					ZVAL_STRING(&ret.value, lex_val.kn, 1);
				}
				break;
		}
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_qp_set_string */
PHP_MYSQLND_MS_API void
mysqlnd_qp_set_string(struct st_mysqlnd_query_scanner * scanner, const char * const s, size_t len TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_qp_set_string");
	/* scan_string/scan_bytes expect `yyscan_t`, not `yyscan_t*` */
	yy_scan_bytes(s, len, *((yyscan_t *)scanner->scanner));
	DBG_VOID_RETURN;
}
/* }}} */


#ifdef MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION

/* {{{ mysqlnd_qp_create_parser */
PHP_MYSQLND_MS_API struct st_mysqlnd_query_parser *
mysqlnd_qp_create_parser(TSRMLS_D)
{
	struct st_mysqlnd_query_parser * ret = mnd_ecalloc(1, sizeof(struct st_mysqlnd_query_parser));

	DBG_ENTER("mysqlnd_qp_create_parser");
	DBG_INF_FMT("ret=%p", ret);

	ret->scanner = mysqlnd_qp_create_scanner(TSRMLS_C);
	DBG_INF_FMT("ret->scanner=%p", ret->scanner);

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_qp_free_parser */
PHP_MYSQLND_MS_API void
mysqlnd_qp_free_parser(struct st_mysqlnd_query_parser * parser TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_qp_free_parser");
	if (parser) {
		mysqlnd_qp_free_scanner(parser->scanner TSRMLS_CC);

		zend_llist_clean(&parser->parse_info.where_field_list);
		zend_llist_clean(&parser->parse_info.select_field_list);
		zend_llist_clean(&parser->parse_info.table_list);

		mnd_efree(parser);
	}

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_table_list_dtor */
static void
mysqlnd_ms_table_list_dtor(void * pDest)
{
	struct st_mysqlnd_ms_table_info * table_info = (struct st_mysqlnd_ms_table_info *) pDest;
	TSRMLS_FETCH();
	if (table_info) {
		zend_bool pers = table_info->persistent;
		if (table_info->db)	{
			mnd_pefree(table_info->db, pers);
		}
		if (table_info->table) {
			mnd_pefree(table_info->table, pers);
		}
		if (table_info->org_table) {
			mnd_pefree(table_info->org_table, pers);
		}
	}
}
/* }}} */


/* {{{ mysqlnd_ms_field_list_dtor */
static void
mysqlnd_ms_field_list_dtor(void * pDest)
{
	struct st_mysqlnd_ms_field_info * field_info = (struct st_mysqlnd_ms_field_info *) pDest;
	TSRMLS_FETCH();
	if (field_info) {
		zend_bool pers = field_info->persistent;
		if (field_info->db)	{
			mnd_pefree(field_info->db, pers);
		}
		if (field_info->table) {
			mnd_pefree(field_info->table, pers);
		}
		if (field_info->name) {
			mnd_pefree(field_info->name, pers);
		}
		if (field_info->org_name) {
			mnd_pefree(field_info->org_name, pers);
		}
		if (field_info->custom_data && field_info->free_custom_data) {
			mnd_pefree(field_info->custom_data, pers);
		}
	}
}
/* }}} */

extern int mysqlnd_qp_parse (void * TSRMLS_DC);

/* {{{ mysqlnd_qp_start_parser */
PHP_MYSQLND_MS_API int
mysqlnd_qp_start_parser(struct st_mysqlnd_query_parser * parser, const char * const query, const size_t query_len TSRMLS_DC)
{
	int ret;
	DBG_ENTER("mysqlnd_qp_start_parser");

	mysqlnd_qp_set_string(parser->scanner, query, query_len TSRMLS_CC);

	zend_llist_init(&parser->parse_info.table_list, sizeof(struct st_mysqlnd_ms_table_info),
					(llist_dtor_func_t) mysqlnd_ms_table_list_dtor, parser->parse_info.persistent /* pers */);

	zend_llist_init(&parser->parse_info.select_field_list, sizeof(struct st_mysqlnd_ms_field_info),
					(llist_dtor_func_t) mysqlnd_ms_field_list_dtor, parser->parse_info.persistent /* pers */);

	zend_llist_init(&parser->parse_info.where_field_list, sizeof(struct st_mysqlnd_ms_field_info),
					(llist_dtor_func_t) mysqlnd_ms_field_list_dtor, parser->parse_info.persistent /* pers */);

	parser->parse_info.parse_where = FALSE;
	DBG_INF("let's run the parser");
	ret = mysqlnd_qp_parse(parser TSRMLS_CC);
	{
		zend_llist_position pos;
		struct st_mysqlnd_ms_table_info * tinfo;
		DBG_INF("------ TABLE LIST -------");
		for (tinfo = zend_llist_get_first_ex(&parser->parse_info.table_list, &pos);
			 tinfo;
			 tinfo = zend_llist_get_next_ex(&parser->parse_info.table_list, &pos))
		{
				DBG_INF_FMT("db=[%s] table=[%s] org_table=[%s] statement_type=[%d]",
						tinfo->db? tinfo->db:"n/a",
						tinfo->table? tinfo->table:"n/a",
						tinfo->org_table? tinfo->org_table:"n/a",
						parser->parse_info.statement
					);
		}
	}
	{
		zend_llist_position pos;
		struct st_mysqlnd_ms_field_info * finfo;
		DBG_INF("------ SELECT FIELD LIST -------");
		for (finfo = zend_llist_get_first_ex(&parser->parse_info.select_field_list, &pos);
			 finfo;
			 finfo = zend_llist_get_next_ex(&parser->parse_info.select_field_list, &pos))
		{
				DBG_INF_FMT("db=[%s] table=[%s] name=[%s] org_name=[%s]",
						finfo->db? finfo->db:"n/a",
						finfo->table? finfo->table:"n/a",
						finfo->name? finfo->name:"n/a",
						finfo->org_name? finfo->org_name:"n/a"
					);
		}
	}
	{
		zend_llist_position pos;
		struct st_mysqlnd_ms_field_info * finfo;
		DBG_INF("------ WHERE FIELD LIST -------");
		for (finfo = zend_llist_get_first_ex(&parser->parse_info.where_field_list, &pos);
			 finfo;
			 finfo = zend_llist_get_next_ex(&parser->parse_info.where_field_list, &pos))
		{
				DBG_INF_FMT("db=[%s] table=[%s] name=[%s] org_name=%s op=[%s]",
						finfo->db? finfo->db:"n/a",
						finfo->table? finfo->table:"n/a",
						finfo->name? finfo->name:"n/a",
						finfo->org_name? finfo->org_name:"n/a",
						finfo->custom_data? finfo->custom_data:"n/a"
					);
		}
	}

	DBG_RETURN(ret);
}
/* }}} */

#endif /* MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION */

