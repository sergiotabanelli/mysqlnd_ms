FROM centos:7
LABEL Name=mysqlnd_ms Version=0.0.1
RUN yum update -y
RUN yum group install "Development Tools" -y
RUN yum install -y epel-release \ 
        http://rpms.remirepo.net/enterprise/remi-release-7.rpm  \
        yum-utils \
        libmemcached-devel \
        libxm2-devel
RUN yum --disablerepo=epel -y update ca-certificates
RUN yum install -y php74 php74-php-devel php74-php-pdo php74-php-json php74-php-mysqlnd php74-php-opcache
RUN yum install -y php73 php73-php-devel php73-php-pdo php73-php-json php73-php-mysqlnd php73-php-opcache
RUN yum install -y php72 php72-php-devel php72-php-pdo php72-php-json php72-php-mysqlnd php72-php-opcache
RUN yum install -y php55 php55-php-devel php55-php-pdo php55-php-json php55-php-mysqlnd php55-php-opcache
RUN yum clean all
