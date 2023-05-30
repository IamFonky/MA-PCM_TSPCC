//
//  tspcc.cpp
//
//  Copyright (c) 2022 Marcelo Pasin. All rights reserved.
//

#include "graph.hpp"
#include "path.hpp"
#include "atomic_path.hpp"
#include "tspfile.hpp"
#include "lifo.hpp"

#include <pthread.h>
#include <getopt.h>
#include <unistd.h>

#include <queue>
#include <chrono>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <cstdio>
#include <algorithm>
#include <numeric>
#include <filesystem>

#define COUNT_POP_PUSH 0
#define COUNT_PATH_TEST 0
#define COUNT_PATH_LOCK 0
#define DISPLAY_DEAD_TIMINGS 0

enum Verbosity
{
	VER_NONE = 0,
	VER_GRAPH = 1,
	VER_SHORTER = 2,
	VER_BOUND = 4,
	VER_ANALYSE = 8,
	VER_COUNTERS = 16,
	VER_QUEUE = 32,
	VER_LOG_STAT = 64,
	VER_LOG_RUNNING = 128,
	VER_ALL = 255
};

static struct
{
	Verbosity verbose;
	int64_t nb_samples;
	int min_nb_threads;
	int max_nb_threads;
	int thread_step;
	int min_queue_size;
	int max_queue_size;
	int queue_step;
	int queue_size;
	std::string output_path;
	int cutoff_depth;
} config;

static struct
{
	AtomicPath *shortest;
	AtomicLifo<Path> *jobs;
	struct
	{
		int verified; // # of paths checked
		int found;	  // # of times a shorter path was found
		int *bound;	  // # of bound operations per level
	} counter;
	int size;
	int total; // number of paths to check
	int *fact;
	std::atomic_int nb_thread_running;
	std::atomic_int nb_thread_dead;
#if COUNT_POP_PUSH
	std::atomic_int nb_pop;
	std::atomic_int nb_push;
#endif
#if COUNT_PATH_TEST
	std::atomic_int nb_path_test;
#endif
#if COUNT_PATH_LOCK
	std::atomic_long nb_path_lock;
#endif
#if DISPLAY_DEAD_TIMINGS
	std::chrono::_V2::system_clock::time_point start_time;
#endif
	int nb_thread;
} global;

static const struct
{
	char RED[6];
	char BLUE[6];
	char ORIGINAL[6];
} COLOR = {
	.RED = {27, '[', '3', '1', 'm', 0},
	.BLUE = {27, '[', '3', '6', 'm', 0},
	.ORIGINAL = {27, '[', '3', '9', 'm', 0},
};

static void branch_and_bound(Path *current)
{
	if (config.verbose & VER_ANALYSE)
		std::cout << "analysing " << current << '\n';

	if (current->leaf())
	{
		// this is a leaf
		current->add(0);
		if (config.verbose & (VER_COUNTERS | VER_LOG_RUNNING))
			global.counter.verified++;
		if (current->distance() < global.shortest->distance())
		{
			if (config.verbose & VER_SHORTER)
				std::cout << "shorter: " << current << '\n';
#if COUNT_PATH_LOCK
			global.nb_path_lock += global.shortest->copyIfShorter(current);
#else
			global.shortest->copyIfShorter(current);
#endif
#if COUNT_PATH_TEST
			global.nb_path_test++;
#endif

			if (config.verbose & VER_COUNTERS)
				global.counter.found++;
		}
		current->pop();
	}
	else
	{
		// not yet a leaf
		if (current->distance() < global.shortest->distance())
		{
			// continue branching
			for (int i = 1; i < current->max(); i++)
			{
				if (!current->contains(i))
				{
					current->add(i);
					// Vérif si queue est dispo
					// si oui, écrire dans la queue
					bool pushed = false;
					if ((current->size() < (current->max() - config.cutoff_depth)) && (global.jobs->get_size() < config.queue_size))
					// if ((global.jobs->get_size() < config.queue_size))
					{
						Path *newPath = new Path(current);
						pushed = global.jobs->push(newPath);
						if (config.verbose & VER_QUEUE)
						{
							pthread_t tid = pthread_self();
							std::cout << "push in queue " << global.jobs->get_size() << '\n';
							std::cout << "path " << newPath << '\n';
							std::cout << "TID : " << tid << "\n";
						}
						if (!pushed)
						{
							delete newPath;
						}
#if COUNT_POP_PUSH
						else
						{
							global.nb_push++;
						}
#endif
					}
					// si il n'y a pas eu de push
					// Continuer de taffer
					if (!pushed)
					{
						branch_and_bound(current);
					}

					current->pop();
				}
			}
		}
		else
		{
			// current already >= shortest known so far, bound
			if (config.verbose & VER_BOUND)
				std::cout << "bound " << current << '\n';
			if (config.verbose & VER_COUNTERS)
				global.counter.bound[current->size()]++;
		}
	}
}

static void *branch_and_bound_task(void *arg)
{
	bool running = false;
	do
	{
		Path *job_to_do = global.jobs->pop();
		while (job_to_do != nullptr)
		{
#if COUNT_POP_PUSH
			global.nb_pop++;
#endif
			if (!running)
			{
				global.nb_thread_running++;
				running = true;
			}
			if (config.verbose & VER_QUEUE)
			{
				std::cout << "pop in queue " << global.jobs->get_size() << '\n';
				std::cout << "path " << job_to_do << '\n';
			}

			branch_and_bound(job_to_do);
			job_to_do = global.jobs->pop();
		}
		if (config.verbose & VER_LOG_RUNNING)
		{
			std::cout << "nb thread running " << global.nb_thread_running << '\n';
			std::cout << "nb thread dead " << global.nb_thread_dead << '\n';
			std::cout << "nb jobs " << global.jobs->get_size() << '\n';
		}
		if (running)
		{
			global.nb_thread_running--;
			running = false;
		}
	} while (global.nb_thread_running > 0);
	global.nb_thread_dead++;
	if (config.verbose & VER_LOG_RUNNING)
	{
#if DISPLAY_DEAD_TIMINGS
		if (global.nb_thread_dead == 1)
		{
			global.start_time = std::chrono::high_resolution_clock::now();
		}
#endif
		std::cout << "nb thread running " << global.nb_thread_running << '\n';
		std::cout << "nb thread dead " << global.nb_thread_dead << '\n';
	}
	return 0;
}

void reset_counters(int size)
{
	global.size = size;
	global.counter.verified = 0;
	global.counter.found = 0;
	global.counter.bound = new int[global.size];
	global.fact = new int[global.size];
	for (int i = 0; i < global.size; i++)
	{
		global.counter.bound[i] = 0;
		if (i)
		{
			int pos = global.size - i;
			global.fact[pos] = (i - 1) ? (i * global.fact[pos + 1]) : 1;
		}
	}
	global.total = global.fact[0] = global.fact[1];
	global.nb_thread_dead = 0;
}

void print_counters()
{
	std::cout << "total: " << global.total << '\n';
	std::cout << "verified: " << global.counter.verified << '\n';
	std::cout << "found shorter: " << global.counter.found << '\n';
	std::cout << "bound (per level):";
	for (int i = 0; i < global.size; i++)
		std::cout << ' ' << global.counter.bound[i];
	std::cout << "\nbound equivalent (per level): ";
	int equiv = 0;
	for (int i = 0; i < global.size; i++)
	{
		int e = global.fact[i] * global.counter.bound[i];
		std::cout << ' ' << e;
		equiv += e;
	}
	std::cout << "\nbound equivalent (total): " << equiv << '\n';
	std::cout << "check: total " << (global.total == (global.counter.verified + equiv) ? "==" : "!=") << " verified + total bound equivalent\n";
}

Graph *parse_args(int argc, char *argv[])
{

	int opt;

	option longopts[] = {
		{"file", required_argument, NULL, 'f'},
		{"verbosity", required_argument, NULL, 'v'},
		{"samples-count", required_argument, NULL, 's'},
		{"thread-count-min", required_argument, NULL, 't'},
		{"thread-count-max", required_argument, NULL, 'T'},
		{"thread-step", required_argument, NULL, 'i'},
		{"queue-size-min", required_argument, NULL, 'q'},
		{"queue-size-max", required_argument, NULL, 'Q'},
		{"queue-step", required_argument, NULL, 'j'},
		{"output-path", required_argument, NULL, 'o'},
		{"cutoff-depth", required_argument, NULL, 'c'},
	};

	Graph *graph = NULL;
	config.verbose = VER_NONE;
	config.nb_samples = 1;
	config.min_nb_threads = 2;
	config.max_nb_threads = -1;
	config.thread_step = 1;
	config.min_queue_size = 10;
	config.max_queue_size = -1;
	config.queue_step = 1;
	config.output_path = "";
	config.cutoff_depth = 0;

	while ((opt = getopt_long(argc, argv, "f:v:s:t:T:q:Q:o:i:j:c:", longopts, 0)) != -1)
	{
		switch (opt)
		{
		case 'f':
			graph = TSPFile::graph(optarg);
			break;
		case 'v':
			config.verbose = (Verbosity)atoi(optarg);
			break;
		case 's':
			config.nb_samples = atoi(optarg);
			break;
		case 't':
			config.min_nb_threads = atoi(optarg);
			break;
		case 'T':
			config.max_nb_threads = atoi(optarg);
			break;
		case 'i':
			config.thread_step = atoi(optarg);
			break;
		case 'q':
			config.min_queue_size = atoi(optarg);
			break;
		case 'Q':
			config.max_queue_size = atoi(optarg);
			break;
		case 'j':
			config.queue_step = atoi(optarg);
			break;
		case 'c':
			config.cutoff_depth = atoi(optarg);
			break;
		case 'o':
			config.output_path = optarg;
			std::filesystem::path p(config.output_path);
			std::filesystem::path dir = p.parent_path();
			if (!std::filesystem::exists(dir))
			{
				printf("The path %s does not exists, creating directory\n", dir.c_str());
				std::filesystem::create_directory(dir);
			}
			break;
		}
	}

	if (config.max_nb_threads < config.min_nb_threads)
		config.max_nb_threads = config.min_nb_threads;
	if (config.thread_step < 1)
		config.thread_step = 1;

	if (config.max_queue_size < config.min_queue_size)
		config.max_queue_size = config.min_queue_size;
	if (config.queue_step < 1)
		config.queue_step = 1;

	if (graph == NULL)
	{
		perror("you need to provide a file name with the option -f\n");
		exit(1);
	}

	return graph;
}

int span_inc(int current_value, int increment_step)
{
	if (current_value < increment_step)
	{
		return increment_step;
	}
	else
	{
		return current_value + increment_step;
	}
}

int main(int argc, char *argv[])
{
	Graph *g = parse_args(argc, argv);

	for (int queue_size = config.min_queue_size; queue_size <= config.max_queue_size; queue_size = span_inc(queue_size, config.queue_step))
	{
		std::stringstream path;
		std::ofstream outfile;
		path << config.output_path << queue_size;
		if (config.output_path != "")
		{
			std::remove(path.str().c_str());
			outfile.open(path.str(), std::ios_base::out);
			outfile << "nb_thread;distance;min_duration;max_duration;avg_duration";
#if COUNT_POP_PUSH
			outfile << ";nb_pop;nb_push";
#endif
			outfile << "\n";
			outfile.close();
		}

		for (int nb_threads = config.min_nb_threads; nb_threads <= config.max_nb_threads; nb_threads = span_inc(nb_threads, config.thread_step))
		{
			global.nb_thread = nb_threads;
			std::deque<int64_t> durations;
			for (int sample = 0; sample < config.nb_samples; ++sample)
			{
#if COUNT_POP_PUSH
				global.nb_pop = 0;
				global.nb_push = 0;
#endif
#if COUNT_PATH_TEST
				global.nb_path_test = 0;
#endif
#if COUNT_PATH_LOCK
				global.nb_path_lock = 0;
#endif

				config.queue_size = queue_size;

				auto start = std::chrono::high_resolution_clock::now();
				if (config.verbose != VER_LOG_STAT && config.verbose != VER_NONE)
				{
					std::cout << "number of threads " << nb_threads << '\n';
					std::cout << "size of queue " << config.queue_size << '\n';
				}

				if (config.verbose & VER_GRAPH)
					std::cout << COLOR.BLUE << g << COLOR.ORIGINAL;

				if (config.verbose & (VER_COUNTERS | VER_LOG_RUNNING))
					reset_counters(g->size());

				global.shortest = new AtomicPath(g);
				for (int i = 0; i < g->size(); i++)
				{
					global.shortest->add(i);
				}
				global.shortest->add(0);

				Path *current = new Path(g);
				current->add(0);

				pthread_t *workers = new pthread_t[nb_threads];

				global.jobs = new AtomicLifo<Path>(config.queue_size);
				global.jobs->push(current);
				global.nb_thread_dead = 0;

				pthread_create(workers, NULL, branch_and_bound_task, NULL);
				for (int i = 1; i < nb_threads; ++i)
				{
					pthread_create(&(workers[i]), NULL, branch_and_bound_task, NULL);
				}

				for (int i = 0; i < nb_threads; ++i)
				{
					pthread_join(workers[i], NULL);
				}

				if (config.verbose != VER_LOG_STAT && config.verbose != VER_NONE)
					std::cout << COLOR.RED << "shortest " << global.shortest << COLOR.ORIGINAL << '\n';

				if (config.verbose & VER_COUNTERS)
					print_counters();

				auto stop = std::chrono::high_resolution_clock::now();
				int64_t duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
				durations.push_front(duration);
				if (config.verbose & VER_LOG_STAT)
				{
					std::cout << nb_threads;
					std::cout << ";" << config.queue_size;
					std::cout << ";" << global.shortest->distance();
					std::cout << ";" << duration;
#if COUNT_POP_PUSH
					std::cout << ";" << global.nb_pop;
					std::cout << ";" << global.nb_push;
#endif
#if COUNT_PATH_TEST
					std::cout << ";" << global.nb_path_test;
#endif
#if COUNT_PATH_LOCK
					std::cout << ";" << global.nb_path_lock;
#endif
					std::cout << "\n";
#if DISPLAY_DEAD_TIMINGS
					auto stop = std::chrono::high_resolution_clock::now();
					int64_t duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - global.start_time).count();
					std::cout << "Time between first dead thread and last : " << duration << "[μs]\n";
#endif
				}
				delete current;
				delete workers;
			}

			if (config.output_path != "")
			{
				int64_t min_duration = *std::min_element(durations.begin(), durations.end());
				int64_t max_duration = *std::max_element(durations.begin(), durations.end());
				int64_t avg_duration = std::accumulate(durations.begin(), durations.end(), (int64_t)0) / config.nb_samples;

				std::ofstream outfile;
				outfile.open(path.str(), std::ios_base::app);

				outfile << nb_threads << ";";
				outfile << global.shortest->distance() << ";";
				outfile << min_duration << ";";
				outfile << max_duration << ";";
#if COUNT_POP_PUSH
				outfile << global.nb_pop << ";";
				outfile << global.nb_push << ";";
#endif
#if COUNT_PATH_TEST
				outfile << global.nb_path_test << ";";
#endif
				outfile << avg_duration << "\n";

				outfile.close();
			}
		}
	}

	delete global.shortest;

	return 0;
}