PHP_ARG_ENABLE(mysqlnd_ms, whether to enable mysqlnd_ms support,
[  --enable-mysqlnd-ms           Enable mysqlnd_ms support])

PHP_ARG_ENABLE(mysqlnd_ms_table_filter, whether to enable table filter in mysqlnd_ms,
[  --enable-mysqlnd-ms-table-filter   Enable support for table filter in mysqlnd_ms (EXPERIMENTAL - do not use!)], no, no)
PHP_ARG_ENABLE(mysqlnd_ms_cache_support, whether to query caching through mysqlnd_qc in mysqlnd_ms,
[  --enable-mysqlnd-ms-cache-support   Enable query caching through mysqlnd_qc (BETA for mysqli - try!, EXPERIMENTAL for all other - do not use!)], no,
no)

if test -z "$PHP_LIBXML_DIR"; then
  PHP_ARG_WITH(libxml-dir, libxml2 install dir,
    [  --with-libxml-dir=DIR     mysqlnd_ms: libxml2 install prefix], no, no)
fi

if test -z "$PHP_LIBMEMCACHED_DIR"; then
  PHP_ARG_WITH(libmemcached-dir,  for libmemcached,
    [  --with-libmemcached-dir[=DIR]   Set the path to libmemcached install prefix], no, no)
fi

if test "$PHP_MYSQLND_MS" && test "$PHP_MYSQLND_MS" != "no"; then
  if test "$PHP_LIBXML" = "no"; then
    AC_MSG_ERROR([mysqlnd_ms requires LIBXML extension])
  fi


  if test "$PHP_LIBMEMCACHED_DIR" != "no" && test "$PHP_LIBMEMCACHED_DIR" != "yes"; then
    if test -r "$PHP_LIBMEMCACHED_DIR/include/libmemcached/memcached.h"; then
      PHP_LIBMEMCACHED_DIR="$PHP_LIBMEMCACHED_DIR"
    else
      AC_MSG_ERROR([Can't find libmemcached headers under "$PHP_LIBMEMCACHED_DIR"])
    fi
  else
    PHP_LIBMEMCACHED_DIR="no"
    for i in /usr /usr/local; do
      if test -r "$i/include/libmemcached/memcached.h"; then
        PHP_LIBMEMCACHED_DIR=$i
        break
      fi
    done
  fi

  AC_MSG_CHECKING([for libmemcached location])
  if test "$PHP_LIBMEMCACHED_DIR" = "no"; then
    AC_MSG_ERROR([mysqlnd_memcached support requires libmemcached. Use --with-libmemcached-dir=<DIR> to specify the prefix where libmemcached headers and library are located])
  else
    AC_MSG_RESULT([$PHP_LIBMEMCACHED_DIR])
    PHP_LIBMEMCACHED_INCDIR="$PHP_LIBMEMCACHED_DIR/include"
    PHP_ADD_INCLUDE($PHP_LIBMEMCACHED_INCDIR)
    PHP_ADD_LIBRARY_WITH_PATH(memcached, $PHP_LIBMEMCACHED_DIR/$PHP_LIBDIR, MYSQLND_MS_SHARED_LIBADD)
  fi

  PHP_SUBST(MYSQLND_MS_SHARED_LIBADD)

  mysqlnd_ms_sources="php_mysqlnd_ms.c mysqlnd_ms.c mysqlnd_ms_switch.c mysqlnd_ms_config_json.c \
                  mf_wcomp.c mysqlnd_query_lexer.c \
                  mysqlnd_ms_filter_random.c mysqlnd_ms_filter_round_robin.c \
                  mysqlnd_ms_filter_user.c mysqlnd_ms_filter_qos.c \
                  mysqlnd_ms_lb_weights.c mysqlnd_ms_filter_groups.c \
                  fabric/mysqlnd_fabric.c fabric/mysqlnd_fabric_parse_xml.c \
                  fabric/mysqlnd_fabric_strategy_direct.c \
                  fabric/mysqlnd_fabric_strategy_dump.c \
                  fabric/mysqlnd_fabric_php.c \
                  mysqlnd_ms_xa.c mysqlnd_ms_xa_store_mysql.c \
                  mysqlnd_ms_conn_pool.c"

  if test "$PHP_MYSQLND_MS_TABLE_FILTER" && test "$PHP_MYSQLND_MS_TABLE_FILTER" != "no"; then
    AC_DEFINE([MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION], 1, [Enable table partition support])
    mysqlnd_ms_sources="$mysqlnd_ms_sources mysqlnd_query_parser.c mysqlnd_ms_filter_table_partition.c"
  fi

  if test "$PHP_MYSQLND_MS_CACHE_SUPPORT" && test "$PHP_MYSQLND_MS_CACHE_SUPPORT" != "no"; then
    AC_MSG_CHECKING([for mysqlnd_qc header location]);

    PHP_ADD_EXTENSION_DEP(mysqlnd_ms, mysqlnd_qc)
    AC_DEFINE([MYSQLND_MS_HAVE_MYSQLND_QC], 1, [Whether mysqlnd_qc is enabled])
  fi

  PHP_SETUP_LIBXML(XMLWRITER_SHARED_LIBADD, [
    PHP_ADD_EXTENSION_DEP(mysqlnd_ms, mysqlnd)
    PHP_ADD_EXTENSION_DEP(mysqlnd_ms, json)
    PHP_NEW_EXTENSION(mysqlnd_ms, $mysqlnd_ms_sources, $ext_shared)
    PHP_INSTALL_HEADERS([ext/mysqlnd_ms/])
    PHP_ADD_MAKEFILE_FRAGMENT
  ], [
    AC_MSG_ERROR([xml2-config not found. Please check your libxml2 installation.])
  ])
fi
