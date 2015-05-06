flapjackfeeder
==============

Nagios/Icinga event broker module (neb) to feed check execution results to the events queue in Flapjack, via Redis.

Known to work on Linux (eg Ubuntu Precise) with:
- Nagios 3.2.3
- Icinga ?


## compiling

``` bash
vagrant@buildbox:$ cd /vagrant
vagrant@buildbox:/vagrant$ git clone https://github.com/redis/hiredis.git
vagrant@buildbox:/vagrant$ cd hiredis
vagrant@buildbox:/vagrant/hiredis$ make hiredis-example
vagrant@buildbox:/vagrant/hiredis$ cd ..
vagrant@buildbox:/vagrant$ git clone https://github.com/flapjack/flapjackfeeder.git
vagrant@buildbox:/vagrant$ cd flapjackfeeder
vagrant@buildbox:/vagrant/flapjackfeeder$ (cd src ; gcc -fPIC -g -O2 -DHAVE_CONFIG_H -DNSCORE -o flapjackfeeder.o flapjackfeeder.c -shared -fPIC ../../hiredis/libhiredis.a ;strip flapjackfeeder.o)
```

## usage

Just configure Nagios/Icinga to load the neb module in *nagios.cfg* by adding the following line.
Alter the redis host and port according to your needs.
``` cfg
broker_module=/tmp/flapjackfeeder.o redis_host=localhost,redis_port=6379
```
