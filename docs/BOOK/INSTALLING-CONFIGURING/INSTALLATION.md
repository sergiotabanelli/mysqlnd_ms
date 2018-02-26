# Installation
>NOTE: `mymysqlnd_ms` It is not available as PECL extension, you need do download or clone it from Github and compile it from source

* Download or clone from Github.
* Install php-devel and php json package for your distribution and PHP version.
* Install libmemcached and libxml2 development packages for your distribution.
* from your cloned or downloaded directory run:

```
cd /path/to/mymysqlnd_ms
phpize
./configure
make
sudo make install
```
If you find any problems open an [issue on Github](https://github.com/sergiotabanelli/mymysqlnd_ms/issues) 
