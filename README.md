flapjackfeeder
==============

Nagios/Icinga event broker module (neb) to feed check execution results to the events queue in Flapjack, via Redis.

Known to work on Linux (eg Ubuntu Precise) with:
- Nagios 3.2.3
- Nagios 4
- Naemon 4
- Icinga 1.x

## versioning

Versioning is done by tagging.
Add a tag into git; that tag will become the version number during compile time via a define in the Makefile.

## compiling

First clone the git repo to get the source.
Then run make to compile.
This will fetch and compile the only dependency (hiredis) for you as well.

``` bash
vagrant@flapjack:$ cd /vagrant
vagrant@flapjack:/vagrant$ git clone https://github.com/flapjack/flapjackfeeder.git
vagrant@flapjack:/vagrant$ cd flapjackfeeder
vagrant@flapjack:/vagrant/flapjackfeeder$ make
```

If you build following the steps in this README, you get something like this:
```
vagrant@flapjack:/vagrant/flapjackfeeder$ ls -l *.o
-rwxr-xr-x 1 vagrant vagrant 56240 Jun 24 18:00 flapjackfeeder3-v0.0.5.o
-rwxr-xr-x 1 vagrant vagrant 56240 Jun 24 18:01 flapjackfeeder4-v0.0.5.o
```
Those are the two versions of the module built for Nagios3 and Nagios4/Naemon4.

## usage

Configure Nagios/Icinga to load the neb module in *nagios.cfg* by adding the following line.
Alter the redis host, port, database number and queue according to your needs.
``` cfg
broker_module=/tmp/flapjackfeeder.o redis_host=localhost,redis_port=6379,redis_database=0,redis_queue=events,timeout=5
```

You can feed multiple target databases by specifying them on the module load line.
```
broker_module=/tmp/flapjackfeeder.o redis_host=localhost,redis_port=6379,redis_database=0,redis_queue=events,redis_host=127.0.0.1,redis_port=6380,redis_database=1,redis_queue=flapjack_events
```

## options

Besides the **redis_host**, **redis_port**, **redis_database** and **redis_queue** options, which can be given multiple times to provide data to more than one redis database, there are more options that you can set.

- **timeout** in seconds (defaults to 1,5 seconds, but you can only specify an integer here)

  *Warning: this will block nagios for the time it is waiting on redis not only once but every $redis_connect_retry_interval seconds!*

- **redis_connect_retry_interval** in seconds (defaults to 15 seconds)

- **flapjack_version** (integer) if > 1, then it will create the extra object on the 'events_actions' queue required for safely interruptible event processing Flapjack v2+.

