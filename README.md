flapjackfeeder
==============

Nagios/Icinga event broker module (neb) to feed the pipe for flapjack-nagios-receiver


## compiling

`` bash
vagrant@buildbox:/vagrant/flapjackfeeder$ (cd src ; gcc -fPIC -g -O2 -DHAVE_CONFIG_H -DNSCORE -o flapjackfeeder.o flapjackfeeder.c -shared   -fPIC)
``
