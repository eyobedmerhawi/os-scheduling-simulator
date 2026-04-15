#include <stdio.h>
#include <stdlib.h>

#define MAX_PROCESSES 100
#define MAX_BLOCKS 200
#define MAX_REQS 200

// ======================= CPU SCHEDULING =======================
typedef struct {
    int pid;
    int arrival;
    int burst;
    int priority;

    int start;
    int finish;
    int waiting;
    int turnaround;

    int done;   // used for SJF selection
} Process;

static void swap(Process *a, Process *b) {
    Process t = *a;
    *a = *b;
    *b = t;
}

static int read_processes(const char *filename, Process p[]) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening processes.txt");
        return -1;
    }

    int n = 0;
    while (n < MAX_PROCESSES &&
           fscanf(fp, "%d %d %d %d",
                  &p[n].pid, &p[n].arrival, &p[n].burst, &p[n].priority) == 4) {
        p[n].start = p[n].finish = p[n].waiting = p[n].turnaround = 0;
        p[n].done = 0;
        n++;
    }
    fclose(fp);
    return n;
}

static void reset_computed(Process p[], int n) {
    for (int i = 0; i < n; i++) {
        p[i].start = p[i].finish = p[i].waiting = p[i].turnaround = 0;
        p[i].done = 0;
    }
}

// ---------- FCFS ----------
static void sort_fcfs(Process p[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (p[j].arrival > p[j + 1].arrival ||
                (p[j].arrival == p[j + 1].arrival && p[j].pid > p[j + 1].pid)) {
                swap(&p[j], &p[j + 1]);
            }
        }
    }
}

static void simulate_fcfs(Process p[], int n) {
    int time = 0;
    for (int i = 0; i < n; i++) {
        if (time < p[i].arrival) time = p[i].arrival;

        p[i].start = time;
        p[i].finish = time + p[i].burst;
        p[i].turnaround = p[i].finish - p[i].arrival;
        p[i].waiting = p[i].start - p[i].arrival;

        time = p[i].finish;
    }
}

// ---------- SJF (non-preemptive) ----------
static void simulate_sjf(Process p[], int n, int order[]) {
    int time = 0;
    int completed = 0;

    while (completed < n) {
        int idx = -1;

        // choose ready process with smallest burst; tie-break arrival then PID
        for (int i = 0; i < n; i++) {
            if (!p[i].done && p[i].arrival <= time) {
                if (idx == -1 ||
                    p[i].burst < p[idx].burst ||
                    (p[i].burst == p[idx].burst && p[i].arrival < p[idx].arrival) ||
                    (p[i].burst == p[idx].burst && p[i].arrival == p[idx].arrival && p[i].pid < p[idx].pid)) {
                    idx = i;
                }
            }
        }

        // if CPU idle, jump to next arriving process
        if (idx == -1) {
            int nextArrival = 1000000000;
            for (int i = 0; i < n; i++) {
                if (!p[i].done && p[i].arrival < nextArrival) {
                    nextArrival = p[i].arrival;
                }
            }
            time = nextArrival;
            continue;
        }

        p[idx].start = time;
        p[idx].finish = time + p[idx].burst;
        p[idx].turnaround = p[idx].finish - p[idx].arrival;
        p[idx].waiting = p[idx].start - p[idx].arrival;
        p[idx].done = 1;

        order[completed] = idx;
        completed++;
        time = p[idx].finish;
    }
}

static void print_gantt_from_order(const char *title, Process p[], int n, int order[]) {
    printf("\nGantt Chart (%s):\n|", title);
    for (int i = 0; i < n; i++) {
        int idx = order[i];
        printf(" P%d |", p[idx].pid);
    }
    printf("\n");

    printf("%d", p[order[0]].start);
    for (int i = 0; i < n; i++) {
        int idx = order[i];
        printf("%4d", p[idx].finish);
    }
    printf("\n");
}

static void print_metrics(const char *title, Process p[], int n, int order[]) {
    double sumWT = 0, sumTAT = 0;

    printf("\n%s Metrics:\n", title);
    printf("PID  AT  BT  WT  TAT\n");

    for (int i = 0; i < n; i++) {
        int idx = order[i];
        printf("%3d %3d %3d %3d %4d\n",
               p[idx].pid, p[idx].arrival, p[idx].burst, p[idx].waiting, p[idx].turnaround);
        sumWT += p[idx].waiting;
        sumTAT += p[idx].turnaround;
    }

    printf("\nAverage WT  = %.2f\n", sumWT / n);
    printf("Average TAT = %.2f\n", sumTAT / n);
}

static double cpu_utilization(Process p[], int n, int order[]) {
    int busy = 0;
    int start = p[order[0]].start;
    int end = p[order[0]].finish;

    for (int i = 0; i < n; i++) {
        int idx = order[i];
        busy += p[idx].burst;
        if (p[idx].start < start) start = p[idx].start;
        if (p[idx].finish > end) end = p[idx].finish;
    }

    int total = end - start;
    if (total <= 0) return 0.0;
    return (busy * 100.0) / total;
}

// ======================= MEMORY (FIRST-FIT) =======================
typedef struct {
    int block_id;
    int size;
    int allocated;   // 0 free, 1 allocated
    int alloc_pid;   // PID that got this block
} Block;

typedef struct {
    int pid;
    int size_needed;
} Request;

static int read_blocks(const char *filename, Block b[]) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening memory.txt");
        return -1;
    }

    int n = 0;
    while (n < MAX_BLOCKS) {
        int id, size;
        int r = fscanf(fp, "%d %d", &id, &size);
        if (r == 2) {
            b[n].block_id = id;
            b[n].size = size;
            b[n].allocated = 0;
            b[n].alloc_pid = -1;
            n++;
        } else {
            int c = fgetc(fp);
            if (c == EOF) break;
        }
    }
    fclose(fp);
    return n;
}

static int read_requests(const char *filename, Request req[]) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening alloc_requests.txt");
        return -1;
    }

    int n = 0;
    while (n < MAX_REQS) {
        int pid, need;
        int r = fscanf(fp, "%d %d", &pid, &need);
        if (r == 2) {
            req[n].pid = pid;
            req[n].size_needed = need;
            n++;
        } else {
            int c = fgetc(fp);
            if (c == EOF) break;
        }
    }
    fclose(fp);
    return n;
}

static void run_first_fit(void) {
    Block blocks[MAX_BLOCKS];
    Request reqs[MAX_REQS];

    int nb = read_blocks("memory.txt", blocks);
    int nr = read_requests("alloc_requests.txt", reqs);

    if (nb <= 0 || nr <= 0) {
        printf("\n[Memory] Skipping First-Fit (missing/empty memory.txt or alloc_requests.txt)\n");
        return;
    }

    printf("\n==================== Memory Allocation (First-Fit) ====================\n");
    printf("Blocks: %d, Requests: %d\n\n", nb, nr);
    printf("PID  Req  Block  BlockSize  Frag  Status\n");

    int total_frag = 0;

    for (int i = 0; i < nr; i++) {
        int pid = reqs[i].pid;
        int need = reqs[i].size_needed;

        int chosen = -1;
        for (int j = 0; j < nb; j++) {
            if (!blocks[j].allocated && blocks[j].size >= need) {
                chosen = j; // first fit
                break;
            }
        }

        if (chosen == -1) {
            printf("%3d %4d %5s %9s %5s  FAILED\n", pid, need, "-", "-", "-");
        } else {
            blocks[chosen].allocated = 1;
            blocks[chosen].alloc_pid = pid;

            int frag = blocks[chosen].size - need; // internal fragmentation
            total_frag += frag;

            printf("%3d %4d %5d %9d %5d  OK\n",
                   pid, need, blocks[chosen].block_id, blocks[chosen].size, frag);
        }
    }

    printf("\nTotal internal fragmentation = %d\n", total_frag);

    printf("\nFinal block allocation:\n");
    printf("BlockID  Size  Status\n");
    for (int j = 0; j < nb; j++) {
        if (blocks[j].allocated)
            printf("%6d %5d  Allocated to P%d\n", blocks[j].block_id, blocks[j].size, blocks[j].alloc_pid);
        else
            printf("%6d %5d  Free\n", blocks[j].block_id, blocks[j].size);
    }
}

// ======================= PAGING (FIFO) =======================
static void run_fifo_paging(void) {
    FILE *fp = fopen("pages.txt", "r");
    if (!fp) {
        perror("Error opening pages.txt");
        printf("\n[Paging] Skipping FIFO paging (missing pages.txt)\n");
        return;
    }

    int framesCount;
    if (fscanf(fp, "%d", &framesCount) != 1 || framesCount <= 0 || framesCount > 1000) {
        printf("\n[Paging] Invalid frame count in pages.txt\n");
        fclose(fp);
        return;
    }

    int *frames = (int *)malloc(sizeof(int) * framesCount);
    if (!frames) {
        printf("\n[Paging] Memory allocation failed\n");
        fclose(fp);
        return;
    }

    for (int i = 0; i < framesCount; i++) frames[i] = -1;

    int fifoIndex = 0;
    int faults = 0, hits = 0, refs = 0;

    printf("\n==================== Paging (FIFO Page Replacement) ====================\n");
    printf("Frames = %d\n", framesCount);
    printf("Ref  | Result | Frames\n");
    printf("-----+--------+----------------\n");

    int page;
    while (fscanf(fp, "%d", &page) == 1) {
        refs++;

        int hit = 0;
        for (int i = 0; i < framesCount; i++) {
            if (frames[i] == page) { hit = 1; break; }
        }

        if (hit) {
            hits++;
            printf("%4d |  HIT   | ", page);
        } else {
            faults++;
            frames[fifoIndex] = page;
            fifoIndex = (fifoIndex + 1) % framesCount;
            printf("%4d | FAULT  | ", page);
        }

        for (int i = 0; i < framesCount; i++) {
            if (frames[i] == -1) printf("- ");
            else printf("%d ", frames[i]);
        }
        printf("\n");
    }

    fclose(fp);
    free(frames);

    if (refs == 0) {
        printf("\nNo page references found in pages.txt.\n");
        return;
    }

    printf("\nTotal References = %d\n", refs);
    printf("Page Faults      = %d\n", faults);
    printf("Hits             = %d\n", hits);
    printf("Hit Ratio        = %.2f%%\n", (hits * 100.0) / refs);
    printf("Fault Rate       = %.2f%%\n", (faults * 100.0) / refs);
}

int main(void) {
    // ---------- CPU Scheduling ----------
    Process base[MAX_PROCESSES];
    int n = read_processes("processes.txt", base);
    if (n < 0) return 1;
    if (n == 0) {
        printf("No processes read. Check processes.txt.\n");
        return 1;
    }

    // FCFS
    Process fcfs[MAX_PROCESSES];
    for (int i = 0; i < n; i++) fcfs[i] = base[i];

    sort_fcfs(fcfs, n);
    simulate_fcfs(fcfs, n);

    int fcfsOrder[MAX_PROCESSES];
    for (int i = 0; i < n; i++) fcfsOrder[i] = i;

    print_gantt_from_order("FCFS", fcfs, n, fcfsOrder);
    print_metrics("FCFS", fcfs, n, fcfsOrder);
    printf("CPU Utilization (FCFS) = %.2f%%\n", cpu_utilization(fcfs, n, fcfsOrder));

    // SJF
    Process sjf[MAX_PROCESSES];
    for (int i = 0; i < n; i++) sjf[i] = base[i];
    reset_computed(sjf, n);

    int sjfOrder[MAX_PROCESSES];
    simulate_sjf(sjf, n, sjfOrder);

    print_gantt_from_order("SJF (Non-preemptive)", sjf, n, sjfOrder);
    print_metrics("SJF", sjf, n, sjfOrder);
    printf("CPU Utilization (SJF)  = %.2f%%\n", cpu_utilization(sjf, n, sjfOrder));

    // ---------- Bonus Memory + Paging ----------
    run_first_fit();
    run_fifo_paging();

    return 0;
}
