PHP_TEST_MYSQLND_MS_MODDEP = posix iconv json pdo mysqlnd mysqlnd_mysql mysql mysqlnd_mysqli mysqli pdo_mysqlnd pdo_mysql mysqlnd_qc

PHP_TEST_MYSQLND_EXTENSION = ` \
        if test "x$(PHP_TEST_MYSQLND_MS_MODDEP)" != "x"; then \
                for i in $(PHP_TEST_MYSQLND_MS_MODDEP)""; do \
                        if test -f $(INSTALL_ROOT)$(EXTENSION_DIR)/$$i.$(SHLIB_DL_SUFFIX_NAME); then \
                                echo -n " -d extension=$(INSTALL_ROOT)$(EXTENSION_DIR)/$$i.$(SHLIB_DL_SUFFIX_NAME) "; \
                        fi; \
                done; \
        fi`
PHP_TEST_SHARED_EXTENSIONS := -d 'mysqlnd_ms.master_on=' -d mysqlnd_ms.debug=d:t:x:A,$(top_builddir)/test_mysqlnd.trace $(PHP_TEST_MYSQLND_EXTENSION)$(PHP_TEST_SHARED_EXTENSIONS)
TESTSBASE=$(addsuffix .log,$(basename $(TESTS)))

cleantrace:
	rm -f $(top_builddir)/test_mysqlnd.trace

showtestslog:
	@if test -f "$(TESTSBASE)"; then \
		tail -n +1 "$(TESTSBASE)"; \
	fi

testtest: cleantrace test showtestslog

#php -n -c ./tmp-php.ini -d 'mysqlnd_ms.enable=0' -d 'open_basedir=' -d 'output_buffering=0' -d 'memory_limit=-1' ./run-tests.php -P -n -c ./tmp-php.ini -d extension_dir=./modules/ -d 'mysqlnd_ms.debug=d:t:x:A,/tmp/mysqlnd.test.trace' -d 'extension=/usr/lib64/php/modules/mysqlnd.so'  -d 'extension=/usr/lib64/php/modules/pdo.so' -d 'extension=/usr/lib64/php/modules/mysqlnd_mysql.so'  -d 'extension=/usr/lib64/php/modules/mysqlnd_mysqli.so'  -d 'extension=/usr/lib64/php/modules/pdo_mysqlnd.so' -d 'extension=/usr/lib64/php/modules/json.so'  -d 'extension=mysqlnd_ms.so' $1.phpt

