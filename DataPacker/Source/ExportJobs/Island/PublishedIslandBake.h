#pragma once

struct IslandBakeContext;
struct RouteSubdivision;

bool UsePublishedIslandBake(const IslandBakeContext& rContext, const RouteSubdivision& rRoute, int64_t iBakeVersion, int64_t iSplitVersion);
void PublishIslandBake(const IslandBakeContext& rContext, const RouteSubdivision& rRoute, int64_t iBakeVersion, int64_t iSplitVersion);
void CopyIslandBakeGeometry(const std::filesystem::path& rRouteDirectory, int64_t iLeafCount);
void FinalizePublishedIslandBakes();
bool IsPublishedIslandLeaf(const std::filesystem::path& rLeaf);

void ResetPublishedIslandBakes();
