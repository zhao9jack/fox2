Note:
It's recommended to use the http port 80 to provide fox2 service. You can create a proxy to fox2server for 
the domain that fox2 uses if don't want fox2server to bind the port 80. Good luck! :-)

For linux users, you have to compile the source code using the following commands:

make -f linux.mak
./plugin_admin.sh
./plugin_fox.sh


The fox2client and fox2server both use cwebserver and plugin_fox, but with different configuration files.


If you have some troubles in configuring the applications, read source code please!