print("=== cat /proc/filesystems\n");
var results = execProcess("/bin/cat", ["/proc/filesystems"]);

print("results.pid=" + results.pid + "\n");
print("results.status=" + results.status + "\n");
print("results.stdout=\"" + results.stdout + "\"\n");
print("results.stderr=\"" + results.stderr + "\"\n");

print("\n=== /bin/dog /proc/filesystems\n");
results = execProcess("/bin/dog", ["/proc/filesystems"]);

print("results.pid=" + results.pid + "\n");
print("results.status=" + results.status + "\n");
print("results.stdout=\"" + results.stdout + "\"\n");
print("results.stderr=\"" + results.stderr + "\"\n");

print("\n=== /bin/cat /bin/dog\n");
results = execProcess("/bin/cat", ["/bin/dog"]);

print("results.pid=" + results.pid + "\n");
print("results.status=" + results.status + "\n");
print("results.stdout=\"" + results.stdout + "\"\n");
print("results.stderr=\"" + results.stderr + "\"\n");

