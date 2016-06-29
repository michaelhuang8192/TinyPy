# TinyPy
standalone python engine with windows service embedded


<blockquote>
<p>
This is a portable Python program that's very helpful for creating small tools. It also can run as windows service.
</p>
</blockquote>

<h4><strong>Usage:</strong></h4>
<pre>
--app app_type: shell or service, if not set, it runs main.py which is located in appdir
--appdir current_working_directory: if not set, the program path is used.
--console: show console

for service only:
--action action_type: install or remove
--service-name name: windows service name
--service-args arguments: arguments are passed the service instance

TinyPy.stopPending() returns true when there's a stop request
</pre>
<br />

<h4><strong>Example:</strong></h4>
<pre>

open shell:
TinyPy.exe --app shell

create service:
TinyPy.exe --app service --action install --service-name test --service-args &quot;arg0=123&quot;

run script:
#create a main script named 'main.py'
TinyPy.exe

#if the main script is located in path 'c:\www'
TinyPy.exe --appdir &quot;c:\www&quot;

</pre>
