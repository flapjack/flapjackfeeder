flapjackfeeder
==============

Nagios/Icinga event broker module (neb) to feed check execution results to the events queue in Flapjack, via Redis.

Known to work on Linux (eg Ubuntu Precise) with:
- Nagios 3.2.3
- Naemon 4
- Icinga ?


## compiling

To compile for Naemon / Nagios 4 add the -DHAVE_NAEMON_H compile time option

``` bash
vagrant@buildbox:$ cd /vagrant
vagrant@buildbox:/vagrant$ git clone https://github.com/redis/hiredis.git
vagrant@buildbox:/vagrant$ cd hiredis
vagrant@buildbox:/vagrant/hiredis$ make hiredis-example
vagrant@buildbox:/vagrant/hiredis$ cd ..
vagrant@buildbox:/vagrant$ git clone https://github.com/flapjack/flapjackfeeder.git
vagrant@buildbox:/vagrant$ cd flapjackfeeder
# for old Nagios
vagrant@buildbox:/vagrant/flapjackfeeder$ (export VERSION=$(awk -F\" '/NEBMODULE_MODINFO_VERSION/ {print $2}' src/flapjackfeeder.c) ; cd src ; gcc -fPIC -g -O2 -DHAVE_CONFIG_H -DNSCORE -o flapjackfeeder3-${VERSION}.o flapjackfeeder.c -shared -fPIC ../../hiredis/libhiredis.a ;strip flapjackfeeder3-${VERSION}.o)
# or for Naemon / Nagios 4:
vagrant@buildbox:/vagrant/flapjackfeeder$ (export VERSION=$(awk -F\" '/NEBMODULE_MODINFO_VERSION/ {print $2}' src/flapjackfeeder.c) ; cd src ; gcc -fPIC -g -O2 -DHAVE_NAEMON_H -DHAVE_CONFIG_H -DNSCORE -o flapjackfeeder4-${VERSION}.o flapjackfeeder.c -shared -fPIC ../../hiredis/libhiredis.a ;strip flapjackfeeder4-${VERSION}.o)
vagrant@buildbox:/vagrant/flapjackfeeder$ ls -l src/*.o
```

If you build following the steps in this README, you get something like this:
```
vagrant@flapjack:/vagrant/flapjackfeeder$ ls -l src/*.o
-rwxr-xr-x 1 vagrant vagrant 56240 Jun 24 18:00 src/flapjackfeeder3-0.0.5.o
-rwxr-xr-x 1 vagrant vagrant 56240 Jun 24 18:01 src/flapjackfeeder4-0.0.5.o
```
Those are the two versions of the module build for Nagios3 and Nagios4/Naemon4.

## usage

Just configure Nagios/Icinga to load the neb module in *nagios.cfg* by adding the following line.
Alter the redis host and port according to your needs.
``` cfg
broker_module=/tmp/flapjackfeeder.o redis_host=localhost,redis_port=6379
```
