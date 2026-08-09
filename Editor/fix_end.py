with open('index_core.cpp', 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if line.strip() == 'cleanupScreen();':
        new_lines.append('void cleanup() { cleanupScreen(); }\n')
    elif line.strip() == 'return 0;' and len(new_lines) > 2700:
        continue
    elif line.strip() == '}' and len(new_lines) > 2700 and 'cleanup' in new_lines[-1]:
        continue
    else:
        new_lines.append(line)

with open('index_core.cpp', 'w') as f:
    f.writelines(new_lines)
