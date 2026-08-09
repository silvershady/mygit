with open('index.cpp', 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if line.startswith('int main() {'):
        lines[i] = 'int legacy_main() {\n'

# Find the while(true) loop
start = -1
for i, line in enumerate(lines):
    if 'int key = readkey();' in line:
        start = i
        break

# The actual key processing starts after `if (key == -1) continue;`
proc_start = start + 3

# Find the end of the while loop
end = -1
for i in range(proc_start, len(lines)):
    if 'return 0;' in lines[i]:
        end = i - 1
        while not lines[end].strip() == '}':
            end -= 1
        break

new_lines = lines[:proc_start]
new_lines.append('    processKey(key);\n')
new_lines.append('  }\n')
new_lines.append('  return 0;\n}\n\n')
new_lines.append('void processKey(int key) {\n')

for i in range(proc_start, end):
    # Change 'continue' to 'return' since it's now a function
    l = lines[i]
    if l.strip() == 'continue;':
        l = l.replace('continue;', 'return;')
    new_lines.append(l)

new_lines.append('}\n')

new_lines.extend(lines[end+1:])

with open('index_core.cpp', 'w') as f:
    f.writelines(new_lines)
