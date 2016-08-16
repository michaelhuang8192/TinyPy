# TinyPy
Standalone Python engine with Windows service embedded

> This is a portable Python program that's very helpful for creating small tools. It also can run as windows service.

# Requirements:
* Python 2.7 Dev
* Visual C++

# Pre-Compiled Binary For Windows:
Check [/bin](https://github.com/michaelhuang8192/TinyPy/tree/master/bin)

#### Usage:

```
--app app_type: shell or service, if not set, it runs main.py which is located in appdir
--appdir current_working_directory: if not set, the program path is used.
--console: show console

for service only:
--action action_type: install or remove
--service-name name: windows service name
--service-args arguments: arguments are passed the service instance

TinyPy.stopPending() returns true when there's a stop request
```

#### Example:

```
# open shell
TinyPy.exe --app shell

# create service
TinyPy.exe --app service --action install --service-name test --service-args "a=1 b=2"

# run the default script 'main.py' located under the same folder
TinyPy.exe

# run script 'main.py' located in folder "c:\www"
TinyPy.exe --appdir "c:\www"
```

# License
Free

