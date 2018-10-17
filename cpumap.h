#ifndef CPUMAP_H
#define CPUMAP_H

#include <sched.h>

#ifdef CPU_MAP_R730
int cpumap(int i, int nproces)
{
	// For R730 server, where CPU cores from two CPUs mixes.
	if (i < 8)
		return (i*2);
	else
		return ((i-8)*2 + 1);
}

#elif CPU_MAP_S24
/* CPU0: 0-5, 12-17 */
/* CPU1: 6-11, 18-23 */
int cpumap(int i, int nproces)
{
	int id = i % 23;
	if (i == 0)
		return 0;
	if (id == 0)
		return 23;
	else if (id >= 1 && id <=5)
		return id;
	else if (id >= 6 && id <= 11)
		return (id + 6);
	else if (id >=12 && id <= 17)
		return (id - 6);
	else if (id >= 18 && id <= 22)
		return id;
}

#elif CPU_MAP_S56
/* CPU0: 0-13, 28-41 */
/* CPU1: 14-27, 42-55 */
int cpumap(int i, int nproces)
{
	int id = i % 55;
	if (i == 0)
		return 0;
	if (id == 0)
		return 55;
	else if (id >= 1 && id <=13)
		return id;
	else if (id >= 14 && id <= 27)
		return (id + 14);
	else if (id >= 28 && id <= 41)
		return (id - 14);
	else if (id >= 42 && id <= 54)
		return id;
}

#elif GUADALUPE_SPREAD
int cpumap(int i, int nprocs)
{
  return (i / 36) * 36 + (i % 2) * 18 + (i % 36 / 2);
}

#elif GUADALUPE_OVERSUB
int cpumap(int i, int nprocs) {
  return (i % 18);
}

#elif GUADALUPE_COMPACT
int cpumap(int i, int nprocs)
{
  return (i % 2) * 36 + i / 2;
}

#elif GUADALUPE_MIC_COMPACT
int cpumap(int i, int nprocs)
{
  return (i + 1) % 228;
}

#elif LES_SPREAD
int cpumap(int i, int nprocs)
{
  return i % 4 * 12 + i / 4 % 12;
}

#elif BIOU_COMPACT
int cpumap(int i, int nprocs)
{
  return (i % 2) * 32 + i / 2;
}

#else
int cpumap(int id, int nprocs)
{
  return id % nprocs;
}

#endif

#endif /* end of include guard: CPUMAP_H */
