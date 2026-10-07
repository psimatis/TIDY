import sys
import re

def parse_log(logfile):
    experiments = []
    current_exp = None
    current_method = None

    with open(logfile, 'r') as f:
        for line in f:
            line = line.strip()

            if line.startswith("Dataset:"):
                if current_exp:
                    experiments.append(current_exp)
                current_exp = {
                    'dataset': line.split(":", 1)[1].strip(),
                    'extent': None,
                    'methods': {}
                }
            elif line.startswith("Extent:") and current_exp:
                current_exp['extent'] = line.split(":", 1)[1].strip()
            elif line.startswith("--- ") and line.endswith(" ---"):
                current_method = line[4:-4]
            elif line.startswith("Total result") and current_exp and current_method:
                match = re.search(r':\s*(\d+)', line)
                if match:
                    current_exp['methods'][current_method] = int(match.group(1))

    if current_exp:
        experiments.append(current_exp)

    return experiments

def verify_experiments(logfile):
    experiments = parse_log(logfile)
    if not experiments:
        print("No experiments found")
        return True

    all_passed = True
    for exp in experiments:
        if len(exp['methods']) < 2:
            print(f"WARNING: Only {len(exp['methods'])} methods ran for {exp['dataset']} {exp['extent']}")
            continue

        results = list(exp['methods'].values())
        if len(set(results)) == 1:
            print(f"PASS: {exp['dataset']} {exp['extent']} - All methods returned {results[0]}")
        else:
            print(f"FAIL: {exp['dataset']} {exp['extent']} - Results differ:")
            for method, result in exp['methods'].items():
                print(f"  {method}: {result}")
            all_passed = False

    return all_passed

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: verify.py <logfile>")
        sys.exit(1)

    if not verify_experiments(sys.argv[1]):
        sys.exit(1)
    sys.exit(0)
