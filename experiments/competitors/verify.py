import sys
import re
from collections import defaultdict

def parse_competitors_log(logfile):
    results = defaultdict(lambda: defaultdict(dict))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    current_method = None
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif line == 'LIT' or line == 'LIT-STAB':
            current_method = 'LIT'
        elif line == 'R-tree' or line == 'R-tree-STAB':
            current_method = 'R-tree'
        elif 'Total result' in line:
            match = re.search(r':\s*(\d+)', line)
            if match and dataset and extent and current_method:
                results[dataset][extent][current_method] = int(match.group(1))
                current_method = None
    
    return results


def parse_brail_variants_log(logfile):
    """Parse brail_variants log, extracting TIDY-Full (full pruning) as B-rail results.
    Dead and live counts are stored separately then summed to match how other methods report totals.
    """
    results = defaultdict(lambda: defaultdict(dict))

    with open(logfile, 'r') as f:
        lines = f.readlines()

    dataset = None
    extent = None
    in_tidy_full = False
    in_live = False
    dead_count = None

    for line in lines:
        line = line.strip()

        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
            in_tidy_full = False
            in_live = False
            dead_count = None
        elif '--- TIDY-Full (full pruning) ---' in line:
            in_tidy_full = True
            in_live = False
            dead_count = None
        elif '--- Live Index ---' in line and in_tidy_full:
            in_tidy_full = False
            in_live = True
        elif line.startswith('--- '):
            in_tidy_full = False
            in_live = False
            dead_count = None
        elif in_tidy_full and 'Total result [COUNT]' in line:
            match = re.search(r':\s*(\d+)', line)
            if match:
                dead_count = int(match.group(1))
        elif in_live and 'Total result (live) [COUNT]' in line:
            match = re.search(r':\s*(\d+)', line)
            if match and dataset and extent and dead_count is not None:
                results[dataset][extent]['B-rail'] = dead_count + int(match.group(1))
            in_live = False
            dead_count = None

    return results

def parse_learned_log(logfile):
    """Parse TIDY-Learned log to use as reference."""
    results = defaultdict(lambda: defaultdict(int))

    with open(logfile, 'r') as f:
        lines = f.readlines()

    dataset = None
    extent = None
    in_learned = False

    for line in lines:
        line = line.strip()

        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
            in_learned = False
        elif line in ('TIDY-LEARNED', 'TIDY-LEARNED-STAB'):
            in_learned = True
        elif in_learned and 'Total result [COUNT]' in line:
            match = re.search(r':\s*(\d+)', line)
            if match and dataset and extent:
                results[dataset][extent] = int(match.group(1))
            in_learned = False

    return results

def verify_experiments(competitors_logfile, learned_logfile, brail_variants_logfile):
    competitors_results = parse_competitors_log(competitors_logfile)
    reference_results = parse_learned_log(learned_logfile)
    brail_results = parse_brail_variants_log(brail_variants_logfile)

    # Merge B-rail results into competitors_results
    for dataset in brail_results:
        for extent in brail_results[dataset]:
            competitors_results[dataset][extent]['B-rail'] = brail_results[dataset][extent]['B-rail']

    # Also add TIDY-Learned itself to the check
    for dataset in reference_results:
        for extent in reference_results[dataset]:
            competitors_results[dataset][extent]['TIDY-Learned'] = reference_results[dataset][extent]

    if not competitors_results:
        print("No experiments found")
        return True

    all_passed = True
    for dataset in competitors_results:
        for extent in competitors_results[dataset]:
            ref = reference_results.get(dataset, {}).get(extent, None)

            if ref is None:
                print(f"WARNING: No TIDY-Learned reference for {dataset} {extent}")
                continue

            for method, result in competitors_results[dataset][extent].items():
                if method == 'TIDY-Learned':
                    continue
                if result == ref:
                    print(f"PASS: {dataset} {extent} - {method}: {result} matches TIDY-Learned")
                else:
                    print(f"FAIL: {dataset} {extent} - {method}: {result} != TIDY-Learned {ref}")
                    all_passed = False

    return all_passed

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: verify.py <competitors_logfile> <learned_logfile> <brail_variants_logfile>")
        sys.exit(1)

    if not verify_experiments(sys.argv[1], sys.argv[2], sys.argv[3]):
        print("\nVERIFICATION FAILED - Results do not match")
        sys.exit(1)
    print("\nVERIFICATION PASSED - All results agree")
    sys.exit(0)
