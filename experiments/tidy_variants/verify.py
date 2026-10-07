import sys
import re
from collections import defaultdict


def parse_results(logfile):
    """Parse result counts from a log file. Returns {dataset: {extent: count}}"""
    results = defaultdict(lambda: defaultdict(int))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif 'Total result [COUNT]' in line and dataset and extent:
            match = re.search(r':\s*(\d+)', line)
            if match:
                results[dataset][extent] = int(match.group(1))
    
    return results


def verify(oracle_log, adaptive_log, learned_log):
    oracle = parse_results(oracle_log)
    adaptive = parse_results(adaptive_log)
    learned = parse_results(learned_log)
    
    all_passed = True
    
    for dataset in oracle:
        for extent in oracle[dataset]:
            oracle_count = oracle[dataset][extent]
            adaptive_count = adaptive.get(dataset, {}).get(extent, 0)
            learned_count = learned.get(dataset, {}).get(extent, 0)
            
            if oracle_count == adaptive_count == learned_count:
                print(f"PASS: {dataset} {extent} - All match ({oracle_count})")
            else:
                print(f"FAIL: {dataset} {extent}")
                print(f"  Oracle:   {oracle_count}")
                print(f"  Adaptive: {adaptive_count}")
                print(f"  Learned:  {learned_count}")
                all_passed = False
    
    return all_passed


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: verify.py <oracle_log> <adaptive_log> <learned_log>")
        sys.exit(1)
    
    if not verify(sys.argv[1], sys.argv[2], sys.argv[3]):
        sys.exit(1)
    sys.exit(0)
