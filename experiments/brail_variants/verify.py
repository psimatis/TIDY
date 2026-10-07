import sys
import re
from collections import defaultdict

def parse_brail_variants_log(logfile):
    results = defaultdict(lambda: defaultdict(dict))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    current_variant = None
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif line.startswith('--- TIDY-NoMax'):
            current_variant = 'nomax'
        elif line.startswith('--- TIDY-Full'):
            current_variant = 'full'
        elif line.startswith('--- Live'):
            current_variant = None
        elif 'Total result [COUNT]' in line and current_variant:
            match = re.search(r':\s*(\d+)', line)
            if match and dataset and extent:
                results[dataset][extent][current_variant] = int(match.group(1))
    
    return results

def verify_experiments(logfile):
    results = parse_brail_variants_log(logfile)
    
    if not results:
        print("No experiments found")
        return True
    
    all_passed = True
    for dataset in results:
        for extent in results[dataset]:
            variants = results[dataset][extent]
            
            nomax = variants.get('nomax', None)
            full = variants.get('full', None)
            
            if nomax is None or full is None:
                print(f"WARNING: Missing variant results for {dataset} {extent}")
                continue
            
            if nomax == full:
                print(f"PASS: {dataset} {extent} - All variants match ({full})")
            else:
                print(f"FAIL: {dataset} {extent} - Results differ:")
                print(f"  TIDY-NoMax: {nomax}")
                print(f"  TIDY-Full: {full}")
                all_passed = False
    
    return all_passed

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: verify.py <logfile>")
        sys.exit(1)
    
    if not verify_experiments(sys.argv[1]):
        sys.exit(1)
    sys.exit(0)
