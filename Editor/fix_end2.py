with open('index_core.cpp', 'r') as f:
    content = f.read()
import re
content = re.sub(r'}\s*$', '', content)
with open('index_core.cpp', 'w') as f:
    f.write(content)
