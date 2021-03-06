### 支持插件的高并发处理缓冲层后台

mevent 基于 nmdb(http://blitiri.com.ar/p/nmdb/) 的架构设计，其功能主要就是为高速应用
场合下提供缓存队列，后台执行。 可以有效缓解压力毛刺。
事件中心以插件的方式支持多个应用，每个应用有处理有一单独的缓存队列和处理线程。
单个后端应用处理缓慢不会影响整个事件中心的处理效率。

应用方提供业务编号, 命令号, 命令参数, 交由后台同步或异步执行相关命令的业务逻辑, 返回对应结果.

整个架构和大部分代码来自 nmdb, 多协议网络数据库.


#### 工作模型

大体上与nmdb一致， 只是加了一个多个后端应用的支持。
扩展了nmdb网络协议（http://blitiri.com.ar/p/nmdb/doc/network.html）的Payload部分。
详细网络协议请参考mevent/README.md

客户端发起请求，请求中包含业务编号、命令号、以及相应的参数。 后台程序在收到请求后:

1. 校验请求包合法性
2. 解出业务编号，并查找后端是否支持此种业务
3. 对支持的业务解出整包数据，放入相应的业务队列
4. 如果是同步请求，给相应处理线程发送业务到达信号，处理线程搞定后发送返回包,
   否则直接返回处理成功给调用者.



#### 容量
由于内存限制，每台单机(2G内存)最大支持缓存100万个操作
后断业务逻辑慢导致缓存队列满时会返回REP_ERR_BUSY
入队列每秒 2万 个左右， 故 mevent 理论上支持业务逻辑无响应时满负荷入列 1 分钟左右



#### 依赖
1, 偷懒一把， 事件中心依赖 clearsilver (http://clearsilver.org/) 的 libneo_utl :)


#### 注意
使用 g_ctime 时请特别注意加上本机的时区设置，详情请参考 mbase/doc/learning/command.txt


### php 扩展安装

* 同编译环境现有开发机器上：

```
    cd /
    tar zcvf libmevent.tar.gz
    /usr/local/include/mevent.h
    /usr/local/include/net-const.h
    /usr/local/lib/libmevent.so
    /usr/local/lib/libneo_utl.a
    /usr/local/include/ClearSilver/
    /etc/mevent/ /usr/local/lib/libstreamhtmlparser.*

    scp mevent/server/ext/* root@destip:/usr/local/src/mevent/
```

* 待安装目标服务器上：

```
    cd /usr/local/src/
    mkdir mevent
    cd mevent
    (scp mevent files from server)
    phpize
    ./configure --with-php-config=`which php-config`
    vim Makefile (add -lmevent -lneo_utl, -I /usr/local/include/ClearSilver/)
    make
    make install

    vim /usr/local/php/etc/php.ini
    + extension = "mevent.so"
    killall -9 php-cgi
    /usr/local/php/bin/spawn-fcgi -a 127.0.0.1 -p 10080 -C 64 -u www -f
    /usr/local/php/bin/php-cgi
```


### daemon/mevent 安装
```
    cd /usr/local/lib

    tar zcvf libdaemon.tar.gz libmnl.so libmevent.so libeii.so libstreamhtmlparser.so*
    libmemcached.so* libjson.so* libmongo-client.so* libjpeg.so* libfcgi.so* libglib-2.0.so*
    libiconv.so* libfreetype.so* libpng12.so* libgd.so* mevent_*.so
```
