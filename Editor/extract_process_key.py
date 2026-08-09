with open('index.cpp', 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if 'int key = readkey();' in line:
        start_idx = i
        break

for i, line in enumerate(lines):
    if line.strip() == 'return 0;' and i > start_idx:
        end_idx = i
        break

# The loop ends just before return 0;
while not lines[end_idx].strip().startswith('}'):
    end_idx -= 1

print(f"Loop starts at {start_idx}, ends near {end_idx}")
