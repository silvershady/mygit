with open('index_core.cpp', 'r') as f:
    content = f.read()

content = content.replace('int legacy_main() {', 'void processKey(int key);\nint legacy_main() {')
content = content.replace('      break;', '      exit(0);')

with open('index_core.cpp', 'w') as f:
    f.write(content)
