import sys

def compare_traces(file1, file2):
    try:
        with open(file1, 'r') as f1, open(file2, 'r') as f2:
            lines1 = f1.readlines()
            lines2 = f2.readlines()
            
            # Compare line by line
            for i, (l1, l2) in enumerate(zip(lines1, lines2)):
                if l1.strip() != l2.strip():
                    print(f"Divergence found at line {i+1}:")
                    print(f"Generated: {l1.strip()}")
                    print(f"Reference: {l2.strip()}")
                    return
            
            # Check for length mismatch
            if len(lines1) != len(lines2):
                print(f"Traces match up to line {min(len(lines1), len(lines2))}, but lengths differ.")
                print(f"Generated lines: {len(lines1)}")
                print(f"Reference lines: {len(lines2)}")
                return

            print("Traces match significantly!")

    except FileNotFoundError as e:
        print(f"Error: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python find_divergence.py <generated_trace> <reference_trace>")
    else:
        compare_traces(sys.argv[1], sys.argv[2])
