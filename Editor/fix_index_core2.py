import sys

with open('index_legacy.cpp', 'r') as f:
    content = f.read()

# 1. Disable legacy main
content = content.replace('int main() {', 'void processKey(int key);\nint legacy_main() {')

# 2. Extract processKey
# Find the start of the key processing loop
start_sig = 'int key = readkey();'
if start_sig not in content:
    print("Cannot find readkey()")
    sys.exit(1)

parts = content.split(start_sig)
before = parts[0] + start_sig
after = parts[1]

# The next 'continue;' is 'if (key == -1) continue;'
after = after.replace('continue;', 'return;', 1)

# Now we find the end of the while(true) loop
# We know it ends with:
#   cleanupScreen();
#   return 0;
# }
end_sig = '  cleanupScreen();\n  return 0;\n}'

if end_sig not in after:
    print("Cannot find end of main")
    sys.exit(1)

after_parts = after.split(end_sig)

loop_body = after_parts[0]
remainder = after_parts[1]

# We need to turn loop_body into processKey(key)
# We know 'while (true) {' was in 'before'. We don't remove it, we just let legacy_main() be empty essentially, or just leave it.
# Actually, let's just make legacy_main just call processKey.

new_content = before + '\n    processKey(key);\n  }\n' + end_sig + '\n\nvoid processKey(int key) {\n' + loop_body + '\n}\n' + remainder

# Also replace 'break;' with 'exit(0);' only inside processKey?
# Actually 'break' in processKey was meant to break the while(true) loop.
# So 'exit(0);' is correct since breaking the loop exited the program.
# Let's replace 'break;' with 'exit(0);' in loop_body.
loop_body = loop_body.replace('break;', 'exit(0);')
loop_body = loop_body.replace('continue;', 'return;')

new_content = before + '\n    processKey(key);\n  }\n' + end_sig + '\n\nvoid processKey(int key) {\n' + loop_body + '\n}\n' + remainder

with open('index_core.cpp', 'w') as f:
    f.write(new_content)
