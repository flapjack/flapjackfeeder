flapjackfeeder
==============

Nagios/Icinga event broker module (neb) to feed check execution results to the events queue in Flapjack, via Redis.

Known to work on Linux (eg Ubuntu Precise) with:
- Nagios 3.2.3
- Nagios 4
- Naemon 4
- Icinga 1.x


## compiling

First clone the git repo to get the source.
Then run make to compile.
This will fetch and compile the only dependency (hiredis) for you as well.

``` bash
vagrant@flapjack:$ cd /vagrant
vagrant@flapjack:/vagrant$ git clone https://github.com/flapjack/flapjackfeeder.git
vagrant@flapjack:/vagrant$ cd flapjackfeeder
vagrant@flapjack:/vagrant/flapjackfeeder$ VERSION=$(awk -F\" '/NEBMODULE_MODINFO_VERSION/ {print $2}' src/flapjackfeeder.c) make
```

If you build following the steps in this README, you get something like this:
```
vagrant@flapjack:/vagrant/flapjackfeeder$ ls -l *.o
-rwxr-xr-x 1 vagrant vagrant 56240 Jun 24 18:00 flapjackfeeder3-0.0.5.o
-rwxr-xr-x 1 vagrant vagrant 56240 Jun 24 18:01 flapjackfeeder4-0.0.5.o
```
Those are the two versions of the module build for Nagios3 and Nagios4/Naemon4.

## usage

Just configure Nagios/Icinga to load the neb module in *nagios.cfg* by adding the following line.
Alter the redis host and port according to your needs.
``` cfg
broker_module=/tmp/flapjackfeeder.o redis_host=localhost,redis_port=6379
```
