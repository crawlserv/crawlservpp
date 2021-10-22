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

    mysql -v

### 1.2 Setting up MySQL server



### 1.3 (Optional) Changing the directory of the database

