mevent - high concurrency middleware (cache with app logic, and message queue)

moon is a highy concurrency dynamic web solution write in c.
(currently by bigmaliang@gmail.com)



#### The main skeleton of moon ####

![skeleton](https://raw.githubusercontent.com/bigml/mbase/master/doc/pic/skeleton.png)



#### The moon's code modules ####

![modules](https://raw.githubusercontent.com/bigml/mbase/master/doc/pic/module.png)



### mevent ###
Mevent is the middleware between mgate(fastcgi worker) and database.

Mevent depend on mbase, so you need to clone it with --recursive option.
And read the mbase's README first.

Mevent can work under Sync, or Async mode with udp, or tcp connection.
You need make choise to compact with your businees application.

Most code stolen from [nmdb](https://blitiri.com.ar/p/nmdb/), thanks alot.


#### network skeleton of mevent ####
![network](https://raw.githubusercontent.com/bigml/mbase/master/doc/pic/detail.png)



### protocol ###

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Header
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |    Version    |                 Request ID                    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |         Request command       |             Flags             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                      plugin name length                       |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    :                          plugin name                          :
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    Body
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         variable type                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                      variable name length                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    :                          variable name                        :
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |        variable value/ variable value length/ array count     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    :                          variable value                       :
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

##### request header #####

*   Version (4 bits)

    protocol version

*   Request ID (28 bits)

    request ident number

*   Request command (16 bits)

    request command number (business application command)

*   Flags (16 bits)

    Sync, or Async mode

*   plugin name length (32 bits)

    business application name length

*   plugin name (variable length)

    business application name


##### request body #####

*   variable type (32 bits)
```c
  enum {
      DATA_TYPE_EOF = 0,
      DATA_TYPE_U32,
      DATA_TYPE_ULONG,
      DATA_TYPE_STRING,
      DATA_TYPE_ARRAY
  };
```

*   variable name length (32 bits)

    length of the variable name.


*   variable name (32 bits)

    variable name

*   variable value/ variable value length/ array count (variable length)
```c
  if (variable type == DATA_TYPE_U32 || type == DATA_TYPE_ULONG)
      this means variable value
  else if (variable type == DATA_TYPE_STRING)
      this means variable value length
  else if (variable type == DATA_TYPE_ARRAY)
      this means array count
```

*   variable value (optional, variable length)

    variable value (only appear on variable type == DATA_TYPE_STRING || DATA_TYPE_ARRAY)


Currently, mevent only use a "root" string variable now.
And, it's value is a [hdf string](http://www.clearsilver.net/docs/man_hdf.hdf) for convenience use.


##### response packet #####

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                          Request ID                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                          Reply Code                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                             vsize                             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    :                              val                              :
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*   Request ID (32 bits)

    request ident number (equal to id from request header)

*   Reply Code (32 bits)

    business result

    - REP_ERR_xxx        14 ~ 24  (system level error)
    - REP_ERR_APP_xxx    25 ~ 999 (business level error)
    - REP_OK_xxx         > 1000   (process ok)

*   vsize (optional, 32 bits)

    business return size (if need)

*   val (optional, variable length)

    business return (if need)

    val is a [hdf string](http://www.clearsilver.net/docs/man_hdf.hdf),
    and vsize is it's length.
