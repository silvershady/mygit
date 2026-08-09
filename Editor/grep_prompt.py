with open('index_core.cpp', 'r') as f:
    for i, line in enumerate(f):
        if 'string promptSaveAs()' in line:
            print("Found at", i)
            break
