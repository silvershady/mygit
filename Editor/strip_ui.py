import re

with open('src/core/EditorLogic.cpp', 'r') as f:
    lines = f.readlines()

out = []
skip = False
for i, line in enumerate(lines):
    if line.startswith('void renderHeaderBar(') or line.startswith('void renderHomeScreen(') or line.startswith('void refresh_screen(') or line.startswith('int main('):
        skip = True
    
    if skip and line.startswith('}') and (
        i + 1 == len(lines) or lines[i+1].startswith('//') or lines[i+1].startswith('void') or lines[i+1].strip() == ''
    ):
        # We need to make sure we only stop skipping when the top-level function closes.
        # This is hard to do with just line matching because of nested braces.
        pass

