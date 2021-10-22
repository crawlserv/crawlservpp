# Installation on Ubuntu 20+

**crawlserv++** is an application for crawling websites and analyzing textual content on these websites.

The following step-by-step guide explains the installation on a newly set up Ubuntu 20.04 (or higher) machine. **Note:** It is recommended to first update Ubuntu at least to the newest LTS-version (as of writing, Ubuntu 20.04).

## 1. MySQL server
### 1.1 Installing the MySQL server
For installing the current version of the MySQL server, type the following into the terminal and accept the installation:

    sudo apt install mysql-server

After installation, to check the status of the MySQL server, run the following command:

    service mysql status
    
**Note:** The recommended version of the MySQL server is 8.0 (or higher). Check the version of you MySQL server with:

    mysql -V

### 1.2 Setting up the MySQL server

For setting up the MySQL server, you should first make sure where the configuration files are located. Run

    which mysqld
    
to identify the binary used by the MySQL server, for example:

    /usr/sbin/mysqld

Use this binary to identify the configuration files (replace ```/usr/sbin/mysqld``` with your binary, if necessary):

    /usr/sbin/mysqld --verbose --help | grep -A 1 "Default options"

This should return to you the configuration files used by the MySQL server, for example:

```
Default options are read from the following files in the given order:
/etc/my.cnf /etc/mysql/my.cnf ~/.my.cnf
```

**Note:** Not all of these files need to exist. Pick the file you want to change (or create) and use your favorite command line editor to do so, for example:

    sudo nano /etc/mysql/my.cnf

Add the following lines to the configuration file (or change them, if they already exist), in order to set the default character set to UTF-8 and the maximum packet size to 2 GB:

```
[mysqld]
character-set-server = utf8mb4
max_allowed_packet = 2G
```

**Note:** If you are changing an existing MySQL server, these options might be overwritten by files in the directories included by ```!includedir```. This means you can also add these options to a separate file located in any of these directories.

Optional options that can be included in the configuration file are, for example,

```
innodb_buffer_pool_size = 1G
disable_log_bin
```

to set the InnoDB Buffer Pool Size and disable MySQL binary logging, respectively.

### 1.3 (Optional) Changing the directory of the database

### 1.4 (Optional) Adding additional directories to the database

