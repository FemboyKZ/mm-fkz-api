#include "cs2kz.h"

// Per-player per-mode playtime object for the status report.
//
// TODO(cs2kz): when cs2kz-metamod exposes a per-player mode/time source, replace
// the null values with the seconds accrued in each mode since the last report,
// e.g. {"vnl":12.0,"ckz":3.0}.
//
// For now the keys are shown with null values so the modes are visible.
std::string BuildPlaytimeModesJson(int /*slot*/)
{
	return "{\"cs2kz_vnl\":null,\"cs2kz_ckz\":null}";
}
