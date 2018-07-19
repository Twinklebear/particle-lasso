#pragma once

#include "particle_lasso_cfg.h"
#include "types.h"
#include "import_scivis16.h"
#include "import_uintah.h"
#include "import_xyz.h"
#include "import_cosmic_web.h"
#include "import_pkd.h"
#include "import_gromacs.h"

#ifdef PARTICLE_LASSO_ENABLE_LIDAR
#include "import_las.h"
#endif

namespace pl {

std::vector<ParticleModel> lasso_particles(const FileName &input);

}

