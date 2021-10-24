# Installation on Ubuntu 20+

**crawlserv++** is an application for crawling websites and analyzing textual content on these websites.

The following step-by-step guide explains the installation on a newly set-up Ubuntu 20.04 (or higher).

**Note:** It is recommended to first update Ubuntu at least to the newest LTS-version (Ubuntu 20.04 as of the time of writing).

## 1. Database

### 1.1 Installing the MySQL Server

To enable the latest MySQL repository, run:

```
wget https://dev.mysql.com/get/mysql-apt-config_0.8.19-1_all.deb
sudo dpkg -i mysql-apt-config*
```

Select `Ok` and press `ENTER`.

**Note:** You can also check the official [MySQL APT Repository](https://dev.mysql.com/downloads/repo/apt/) for the file name of the latest package.

For installing the MySQL server, type the following into the terminal and accept the installation:

    sudo apt install mysql-server

After installation, to check the status of the MySQL server, run the following command:

    service mysql status
    
**Note:** The recommended version of the MySQL server is 8.0 (or higher). Check the version of you MySQL server with:

    mysql -V
    
### 1.2 Adding Users to the Database

In order to work correctly, **crawlserv++** needs two MySQL users: one for the frontend, requiring only ```SELECT``` privileges, and one for the command-and-control server requiring extensive privileges.

Create those users by connecting to the MySQL server as root:

    sudo mysql
    
After installation, no root password might be set and you should change that immediately:

    ALTER USER 'root'@'localhost' IDENTIFIED BY 'new_password';
    
Now add users for the frontend and the crawler:

```
CREATE USER 'frontend'@'localhost' IDENTIFIED BY 'frontend-password';
CREATE USER 'crawler'@'localhost' IDENTIFIED BY 'crawler-password';
GRANT SELECT ON *.* TO 'frontend'@'localhost';
GRANT ALL PRIVILEGES ON *.* TO 'crawler'@'localhost';
FLUSH PRIVILEGES;
exit
```

**Warning:** Make sure to use different passwords for crawler and frontend and be aware that the frontend password will be stored in **plain text** in the frontend configuration file.

The crawler password will be prompted every time you start the command-and-control server.
   
For more information about setting up MySQL see the official [MySQL Getting Started guide](https://dev.mysql.com/doc/mysql-getting-started/en/).

### 1.3 Setting up the MySQL Server

For setting up the MySQL server, you should first make sure where the configuration files are located. Run

    which mysqld
    
to identify the binary used by the MySQL server, for example:

    /usr/sbin/mysqld

Use this binary to identify the configuration files (replace `/usr/sbin/mysqld` with your binary, if necessary):

    /usr/sbin/mysqld --verbose --help | grep -A 1 "Default options"

This should return to you the configuration files used by the MySQL server, for example:

```
Default options are read from the following files in the given order:
/etc/my.cnf /etc/mysql/my.cnf ~/.my.cnf
```

**Note:** Not all of these files need to exist. Pick the file you want to change (or create) and use your favorite command line editor to do so, for example:

    sudo nano /etc/mysql/my.cnf

Add the following lines to the configuration file (or change them, if they already exist), in order to set the default character set to UTF-8 and the maximum packet size to the maximum of 1 GB:

```
[mysqld]
character-set-server = utf8mb4
max_allowed_packet = 1G
```

Finally, restart the MySQL server for the changes to take effect:

    sudo service mysql restart
    
 You can check the new options by starting MySQL

**Note:** If you are changing an existing MySQL server, these options might be overwritten by files in the directories included by ```!includedir```. This means you can also add these options to a separate file located in any of these directories.

Optional options that can be included in the configuration file are, for example,

```
innodb_buffer_pool_size = 1G
disable_log_bin
```

to set the InnoDB Buffer Pool Size and disable MySQL binary logging, respectively.

### 1.4 (Optional) Changing the Directory of the Database

If you want to change the directory where the database is located in (e.g. for using the same directory where you want to install the command-and-control server and the frontend or for using an external hard disk), you can change the main directory of the database.

First, you need to locate the configuration file, in which the data directory is or might be defined. It is usually located in one of the included directories in the configuration file(s) identified in step 1.3.

For example: In the configuration file `/etc/mysql/my.cnf` the line `!includedir /etc/mysql/mysql.conf.d/` points to the directory `/etc/mysql/mysql.conf.d/` in which there are several additional configuration files, and in one of those – `/etc/mysql/mysql.conf.d/mysqld.cnf` – the data directory might be defined by removing the corresponding comment starting with `#`:

```
$ grep "\!includedir" /etc/mysql/my.cnf
!includedir /etc/mysql/conf.d/
!includedir /etc/mysql/mysql.conf.d/
$ ls /etc/mysql/conf.d/
mysql.cnf mysqldump.cnf
$ ls /etc/mysql/mysql.conf.d/
mysql.cnf mysqld.cnf
$ grep "datadir" /etc/mysql/conf.d/*.cnf
$ grep "datadir" /etc/mysql/mysql.conf.d/*.cnf
/etc/mysql/mysql.conf.d/mysqld.cnf:# datadir = /var/liv/mysql
```

(This should be the default configuration on a newly set-up Ubuntu 20+ with newly installed MySQL server.)

Open the configuration file with your favorite command line editor, uncomment the line (remove the leading `#`) and set your own data directory, e.g. by using

    sudo nano /etc/mysql/mysql.conf.d/mysqld.cnf

Additionally, you need to redirect AppArmor from the old data directory to the new one. Edit `/etc/apparmor.d/tunables/alias` as root, e.g. by using

    sudo nano /etc/apparmor.d/tunables/alias

Add the following line at the end of the file

    alias /var/lib/mysql/ -> /newpath/mysql/,
    
(Replace `/var/lib/mysql/` with the previous data directory, if applicable, and `/newpath/mysql/` with the new data directory. **Do not** forget the slashes at the end of both paths and the comma at the end of the line!)

Restart the AppArmor profiles with the following command:

    sudo service apparmor restart

Next, change ownership and group of the new data directory (and all contained files and folders) to ```mysql```:

```
sudo chown -R mysql:mysql /newpath/mysql
sudo chmod -R +rw /newpath/mysql
sudo chmod -R g+rw /newpath/mysql
```
    
(Replace `/newpath/mysql` with the new data directory.)

**Note:** If you're using an external drive, make sure that it is mounted with the `users` option, in order to allow access by multiple users. (Check either `/etc/fstabs` or – more securely – `Edit Mount Options...` and then the last input field under `Mount Options`in  Ubuntu's `Disks` app.) Importantly, its mounting point needs to be accessible for reading by `mysql`, too, e.g. by running:

    sudo chmod -R a+r /disk

(Replace `/disk` with the mounting point of the disk.)

Finally, restart the MySQL server for the changes to take effect:

    sudo service mysql restart

### 1.5 (Optional) Adding Additional Directories to the Database

Enabling additional directories for the database is similar to changing the data directory, although those directories need to be explicitly added to the AppArmor profile of MySQL because the alias for the (main) data directory can only used once.

First, add the directory to the configuration file (for locating those files see `1.3 Setting up the MySQL server`.) If no other additional directory is set, you can add the additional directories to any of the configuration files with your favorite terminal text editor, e.g. by using

    sudo nano /etc/mysql/my.cnf

Add the following line in the `[mysqld]` section (containing the server settings added in step 1.3):

    innodb_directories=/newpath/mysql

(Replace `/newpath/mysql` with the additional directory. You can also add multiple directories, just separate them with a semicolon.)

Now add the directories to the AppArmor profile of the MySQL server, e.g. by using:

    sudo nano /etc/apparmor.d/local/usr.sbin.mysqld

For each additional data directory, add the following two lines to the end of the file (you can also add the leading comments if the file did not exist before or is empty):

```
# Site-specific additions and overrides for usr.sbin.mysqld.
# For more details, please see /etc/apparmor.d/local/README.
/newpath/mysql/ r,
/newpath/mysql/** rwk,
```

(Replace `/newpath/mysql` with the additional data directory and do not forget the forward slash at the end of the path as well as the comma at the end of each line.)

Restart the AppArmor profiles with the following command:

    sudo service apparmor restart

Next, change ownership and group of the new data directory (and all contained files and folders) to ```mysql```:

```
sudo chown -R mysql:mysql /newpath/mysql
sudo chmod -R +rw /newpath/mysql
sudo chmod -R g+rw /newpath/mysql
```
    
(Replace `/newpath/mysql` with the parent folder of the new data directory.)

To test whether the MySQL user has the permission to write into the new data directory, you can run:

```
sudo -u mysql touch /newpath/mysql/test
sudo -u mysql rm /newpath/mysql/test
```

**Note:** If you're using an external drive, make sure that it is mounted with the `users` option, in order to allow access by multiple users. (Check either `/etc/fstabs` or – more securely – `Edit Mount Options...` and then the last input field under `Mount Options`in  Ubuntu's `Disks` app.) Importantly, its mounting point needs to be accessible for reading by `mysql`, too, e.g. by running:

    sudo chmod -R a+r /disk

(Replace `/disk` with the mounting point of the disk.)
   
Finally, restart the MySQL server for the changes to take effect:

    sudo service mysql restart

### 1.6 (Optional) Troubleshooting Access to Data Directories

If the MySQL server does not start or is not able to create tables in any of the configured data directories, test whether the MySQL user (`mysql`) has the permission to write into that directory by running:

```
sudo -u mysql touch /path/mysql/test
sudo -u mysql rm /path/mysql/test
```

(Replace `/path/mysql/` with the data directory you want to check access to.)
 
To activate AppArmor notifications (i.e. get notified when AppArmor denies access to any data directory), install and run the following:

```
sudo apt install apparmor-notify
aa-notify -p
```

## 2. Command-and-Control Server

### 2.1 Downloading the Source Code

In order to download the source code from GitHub, you need to have `git` installed on your machine:

    sudo apt install git
    
Move to the directory in which both the source code of the command-and-control server and the frontend should be downloaded into and clone the respository, for example by running:

```
mkdir ~/server
cd ~/server
git clone https://github.com/crawlserv/crawlservpp.git/ .
```

**Note:** The `.`at the end of the cloning command avoids creating an additional subfolder named after the repository (`crawlservpp`).

After downloading, the ouput should look similar to:

```
$ git clone https://github.com/crawlserv/crawlservpp.git/ .
Cloning into '.'...
remote: Enumerating objects: 26428, done.
remote: Counting objects: 100% (2894/2894), done.
remote: Compressing objects: 100% (1185/1185), done.
remote: Total 26428 (delta 2279), reused 2321 (delta 1707), pack-reused 23534
Receiving objects: 100% (26428/26428), 90.35 MiB | 2.70 MiB/s, done.
Resolving deltas: 100% (19524/19524), done.
```

Next, to download the required sub-modules, run:

```
git submodule init
git submodule update
```

### 2.2 Installing Additional Dependencies

Now, install all the additionally required components for building the command-and-control server:

```
sudo apt install build-essential libboost-iostreams-dev libboost-system-dev libaspell-dev libcurl4-libeigen3-dev libmysqlcppconn-dev libpcre2-dev libpugixml-dev libtidy-dev liburiparser-dev libzip-dev openssl-dev zipcmp zipmerge ziptool
```
    
**Note:** If building the command-and-control server (see next step) fails, check the newest version of [`README.md`](https://github.com/crawlserv/crawlservpp/blob/master/README.md) for all the additional components required for building crawlserv++ on your system.

### 2.3 Building the Command-and-Control Server

Before building, change into the directory into which you have downloaded the source code in step 2.1, for example:

   cd ~/server
   
In order to let `git` ignore the directory into which the server will be built, you can run the following commands:

```
echo ".gitignore" >> .gitignore
echo "build/" >> .gitignore
```

Next, run the following commands to create a directory for building and build the server:

```
mkdir build
cd build
cmake ../crawlserv
make
cd ..
```

Finally, to make sure that the command-and-control server has been build (and see its version, licens, and components) run:

    ./build/crawlserv -v
    
***Note:** If you run into trouble building the command-and-control server, first erase the `build` directory before adding additional components and trying to build (starting at `mkdir build`) again:

```
cd ~/server
rm -rd build
```
    
(Replace `~/server` with the directory into which you downloaded the source code in step 2.1.)

### 2.4 Configuring the Command-and-Control Server

To change the configuration, edit the configuration file contained in the subfolder `crawlserv`, e.g. by:

    nano crawlserv/config

**Important:** If you do not wish to use TOR (see next step), remove the value after `network_default_proxy=`.

If you followed the setup in this guide and you want to use the command-and-control server only locally, no further changes are necessary.

### 2.5 (Optional) Installing and Configuring TOR

If you want to enable anonymization functionality via TOR, just install TOR:

    sudo apt install tor
    
If you want the command-and-control server to be able to control TOR, first create a (hashed) control password:

```
tor --hash-password "<password>"
sudo nano /etc/tor/torrc
```
    
Uncomment (remove the initial `#` the  `HashedControlPassword` entry in the configuration file and replace the original entry with the hashed password.

Then, add the original password to the configuration file of the command-and-control server, e.g. by running

    sudo nano ~/server/crawlserv/config
    
and changing the entry starting with `network_tor_control_password`.

### 2.6 Run the Command-and-Control Server

Open a new terminal and go into the sub-directory `creawlserv` of that directory into which you downloaded the source code in step 2.1. Then run the compiled server located in the `../build` directory, passing the default configuration file as argument:

```
cd ~/server/crawlserv
./../build/crawlserv config
```

Enter the password for the crawler as set up in step `1.2 Adding Users to the Database`.

Leave the terminal open as long as the server is 'up and running'. (You can shut it down either by clicking on `Kill server` in the frontend, or by pressing `CTRL+C` in the opened terminal.)

For convenience, you can create a short script to run the server instead:

```
cd ~/server
echo "#/bin/sh" > run.sh
echo "cd crawlserv" > run.sh
echo "./../build/crawlserv config" > run.sh
chmod +x run.sh
```

(Make sure `run.sh` does not exist or is empty beforehand.)

You can then run the server by simply changing into its directory and run `./run.sh`, for example:

```
cd ~/server/crawlserv
./run.sh
```
    
## 3. Frontend

### 3.1 Installing the Apache Server

For installing the current version of the Apache server with PHP support, type the following into the terminal and accept the installation:

    sudo apt install apache2 php php-pear php-dev libapache2-mod-php php-mysql libmcrypt-dev php-mbstring

After installation, to check the status of the MySQL server, run the following command:

    service apache2 status
    
You can also check whether the Apache server is serving the default landing page by opening [http://localhost](http://localhost) on your machine, which should show the `Apache2 Ubuntu Default Page`.

### 3.2 Setting the Directory of the Frontend

The configuration files of the Apache server are located in `/etc/apache2`. The main configuration file is `apache2.conf`, however, it is recommended to use separate configuration files for your configuration. If you freshly installed the server, you can copy the following file – and change the directory in the new file:

```
sudo cp /etc/apache2/sites-enabled/000-default.conf /etc/apache2/sites-enabled/crawlserv.conf
sudo nano /etc/apache2/sites-enabled/crawlserv.conf
```
    
Look for the line starting with `DocumentRoot` and change it so that it points to the sub-directory `crawlserv_frontend` of the directory into which you have downloaded the source code in step `2.1 Downloading the Source Code`, e.g.:

    /home/<user>/server/crawlserv_frontend
    
Enable the new and disable the old configuration file by running:

```
sudo a2ensite crawlserv.conf
sudo a2dissite 000-default.conf
```

Next, edit the main configuration file:

    sudo nano /etc/apache2/apache2.conf
    
Find the section containing entries starting with `<Directory`, which set the permissions for accessing different directories. Add the following entry at the end of the section:

```
<Directory /home/<user>/server/crawlserv_frontend/>
	Options Indexes FollowSymLinks Includes ExecCGI
    AllowOverride All
    Require all granted
</Directory>
```

(Replace `/home/<user>/server/` with the directory into which you've downloaded the source code in step 2.1.)

Restart the server for the changes to take effect:

    sudo service apache2 restart

## 3.3 Configuring the Frontend

To edit the configuration of the frontend, change into the directory into which the source code has been downloaded in step 2.1, and edit the file `config.php` in the sub-directory `crawlserv_frontend/crawlserv/`:

```
cd ~/server
nano crawlserv_frontend/crawlserv/config.php
```

Search for the line starting with `$db_password = ` and replace the fake password with the one that you have set for the frontend user in the database back in step `1.2 Adding Users to the Database`.

If you followed this guide closely, you do not need to change any other options for the frontend to be able to function.

You can check whether the Apache server is serving the frontend by opening [http://localhost](http://localhost) on your machine, which should provide you with access to the frontend.