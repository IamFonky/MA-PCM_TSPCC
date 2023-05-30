//
//  path.hpp
//  
//  Copyright (c) 2022 Marcelo Pasin. All rights reserved.
//

#include <iostream>
#include <cstring>
#include <atomic>
#include "path.hpp"

#ifndef _atomic_path_hpp
#define _atomic_path_hpp

class AtomicPath : public Path {
private:
    std::atomic_flag _lock = ATOMIC_FLAG_INIT;
public:

	AtomicPath(Graph* graph)
	:Path(graph)
	{}
	
	long copyIfShorter(Path* o)
	{
		long nb_spins = 0;
		while (_lock.test_and_set(std::memory_order_acquire)) {
            // Spin while the lock is not available
			nb_spins++;
        }

		if(o->distance() < distance()){
			copy(o);
		}

		_lock.clear(std::memory_order_release); // Release the lock
		return nb_spins;
	}
};
#endif // _atomic_path_hpp