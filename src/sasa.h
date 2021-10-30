#pragma once

#ifndef FAUNUS_SASA_H
#define FAUNUS_SASA_H

#include "space.h"
#include "celllistimpl.h"
#include <range/v3/view/iota.hpp>
#include <unordered_set>

namespace Faunus {

class SASABase {

  protected:
    using Index = AtomData::index_type;

  public:
    struct Neighbours {
        std::vector<size_t> indices;        //!< indices of neighbouring particles in ParticleVector
        PointVector points;                 //!< vectors to neighbouring particles
        Index index;                        //!< index of particle which corresponds to the object
    };

  protected:
    double probe_radius;            //!< radius of the probe sphere
    std::vector<double> areas;      //!< vector holding SASA area of each atom
    std::vector<double> radii;      //!< Radii buffer for all particles
    int slices_per_atom;            //!< number of slices of each sphere in SASA calculation
    const double TWOPI = 2. * M_PI; //!< 2 PI
    const Particle* first_particle; //! first particle in ParticleVector

    /**
     * @brief returns absolute index of particle in ParticleVector
     * @param particle
     */
    size_t indexOf(const Particle& particle) { return static_cast<size_t>(std::addressof(particle) - first_particle); }

    /**
     * @brief Calcuates SASA of a single particle defined by NeighbourData object
     * @param neighbours NeighbourData object of given particle
     * @param radii of the particles in the ParticleVector
     */
    double calcSASAOfParticle(const Neighbours& neighbour) const;

    /**
     * @brief Calcuates total arc length in radians of overlapping arcs defined by two angles
     * @param vector of arcs, defined by a pair (first angle, second angle)
     */
    double exposedArcLength(std::vector<std::pair<double, double>>& arcs) const;

  public:
    /**
     * @brief updates sasa of target particles
     * @param neighbours_data array of NeighbourData objects
     * @param radii of the particles in the ParticleVector
     * @param target_indices absolute indicies of target particles in ParticleVector
     */
    void updateSASA(const std::vector<SASABase::Neighbours>& neighbours_data,
                    const std::vector<size_t>& target_indices);

    /**
     * @brief resizes areas buffer to size of ParticleVector
     * @param space
     */
    virtual void init(Space& spc) = 0;

    /**
     * @brief calculates neighbourData objects of particles specified by target indices in ParticleVector
     * @param space
     * @param target_indices absolute indicies of target particles in ParticleVector
     */
    virtual std::vector<SASABase::Neighbours> calcNeighbourData(Space& spc,
                                                                const std::vector<size_t>& target_indices) = 0;

    /**
     * @brief calculates neighbourData object of a target particle specified by target indiex in ParticleVector
     * @param space
     * @param target_index indicex of target particle in ParticleVector
     */
    virtual SASABase::Neighbours calcNeighbourDataOfParticle(Space& spc, const size_t target_index) = 0;

    virtual void update(Space& spc, const Change& change) = 0;

    const std::vector<double>& getAreas() const { return areas; }

    /**
     * @param spc
     * @param probe_radius in angstrom
     * @param slices_per_atom number of slices of each sphere in sasa calculations
     */
    SASABase(Space& spc, double probe_radius, int slices_per_atom)
        : probe_radius(probe_radius), slices_per_atom(slices_per_atom),
          first_particle(std::addressof(spc.particles.at(0U))) {}
    virtual ~SASABase() {}
};

class SASA : public SASABase {

  public:
    /**
     * @brief resizes areas buffer to size of ParticleVector
     * @param space
     */
    void init(Space& spc) override;

    /**
     * @brief calculates neighbourData objects of particles specified by target indices in ParticleVector
     * @param space
     * @param target_indices absolute indicies of target particles in ParticleVector
     */
    std::vector<SASABase::Neighbours> calcNeighbourData(Space& spc, const std::vector<size_t>& target_indices) override;

    /**
     * @brief calculates neighbourData object of a target particle
     * @brief specified by target index in ParticleVector using O(N^2)
     * @param space
     * @param target_index indicex of target particle in ParticleVector
     */
    SASABase::Neighbours calcNeighbourDataOfParticle(Space& spc, const size_t target_index) override;

    void update(Space& spc, const Change& change) override {}

    /**
     * @param spc
     * @param probe_radius in angstrom
     * @param slices_per_atom number of slices of each sphere in sasa calculations
     */
    SASA(Space& spc, double probe_radius, int slices_per_atom) : SASABase(spc, probe_radius, slices_per_atom) {}
    SASA(const json& j, Space& spc) : SASABase(spc, j.value("radius", 1.4) * 1.0_angstrom, j.value("slices", 20)) {}
};

//!< TODO Figure out how to do it with just CellList_T template argument
//!< TODO update function does perhaps unnecessary containsMember(Member&) checks
//!< TODO finish proper test case
template <typename CellList_T, typename CellCoord> class SASACellList : public SASABase {

  private:
    std::unique_ptr<CellList_T> cell_list; //!< pointer to cell list
    double cell_length;                    //!< dimension of a single cell
    std::vector<CellCoord> cell_offsets;   //!< holds offsets which define a 3x3x3 cube around central cell

  public:
    /**
     * @param spc
     * @param probe_radius in angstrom
     * @param slices_per_atom number of slices of each sphere in sasa calculations
     */
    SASACellList(Space& spc, double probe_radius, int slices_per_atom) : SASABase(spc, probe_radius, slices_per_atom) {}
    SASACellList(const json& j, Space& spc)
        : SASABase(spc, j.value("radius", 1.4) * 1.0_angstrom, j.value("slices", 20)) {}
    virtual ~SASACellList() {}

    /**
     * @brief constructs cell_list with appropriate cell_length and fills it with particles from space
     * @param space
     */
    void init(Space& spc) override {

        cell_offsets.clear();
        for (auto i = -1; i <= 1; ++i) {
            for (auto j = -1; j <= 1; ++j) {
                for (auto k = -1; k <= 1; ++k) {
                    cell_offsets.push_back(CellCoord(i, j, k));
                }
            }
        }

        radii.clear();
        std::for_each(spc.particles.begin(), spc.particles.end(),
                      [&](const Particle& particle) { radii.push_back(particle.traits().sigma * 0.5); });
        auto max_radius = ranges::max(radii);

        cell_length = 2.0 * (max_radius + probe_radius);
        cell_list = std::make_unique<CellList_T>(spc.geometry.getLength(), cell_length);

        for (const auto& particle : spc.activeParticles()) {
            const auto particle_index = indexOf(particle);
            cell_list->insertMember(particle_index, particle.pos + spc.geometry.getLength() / 2.);
        }

        areas.resize(spc.particles.size(), 0.);
    }

    /**
     * @brief calculates neighbourData object of a target particle
     * @brief specified by target index in ParticleVector using cell list
     * @param space
     * @param target_index indicex of target particle in ParticleVector
     */
    SASABase::Neighbours calcNeighbourDataOfParticle(Space& spc, const size_t target_index) override {

        SASABase::Neighbours neighbours;

        const auto& particle_i = spc.particles.at(target_index);
        neighbours.index = target_index;
        const auto sasa_radius_i = radii[target_index] + probe_radius;
        const auto& center_cell = cell_list->getGrid().coordinatesAt(particle_i.pos + spc.geometry.getLength() / 2.);

        auto neighour_particles_at = [&](const CellCoord& offset) {
            return cell_list->getNeighborMembers(center_cell, offset);
        };

        for (const auto& cell_offset : cell_offsets) {

            const auto& neighbour_particle_indices = neighour_particles_at(cell_offset);

            for (const auto neighbour_particle_index : neighbour_particle_indices) {

                const auto& particle_j = spc.particles.at(neighbour_particle_index);
                const auto sasa_radius_j = radii[neighbour_particle_index] + probe_radius;
                const auto sq_cutoff = (sasa_radius_i + sasa_radius_j) * (sasa_radius_i + sasa_radius_j);

                if (target_index != neighbour_particle_index &&
                    spc.geometry.sqdist(particle_i.pos, particle_j.pos) <= sq_cutoff) {

                    const auto dr = spc.geometry.vdist(particle_i.pos, particle_j.pos);
                    neighbours.points.push_back(dr);
                    neighbours.indices.push_back(neighbour_particle_index);
                }
            }
        }

        return neighbours;
    }

    /**
     * @brief calculates neighbourData objects of particles
     * @brief specified by target indices in ParticleVector using cell lists
     * @param space
     * @param target_indices absolute indicies of target particles in ParticleVector
     */
    std::vector<SASABase::Neighbours> calcNeighbourData(Space& spc,
                                                        const std::vector<size_t>& target_indices) override {

        // O(N^2) search for neighbours
        const auto number_of_indices = target_indices.size();
        std::vector<SASA::Neighbours> neighbours(number_of_indices);

        for (size_t i = 0; i != number_of_indices; ++i) {
            neighbours.at(i) = calcNeighbourDataOfParticle(spc, target_indices.at(i));
        }

        return neighbours;
    }

    /**
     * @brief updates cell_list according to change, if the volume changes the cell_list gets rebuilt
     * @param space
     * @param change
     */
    void update(Space& spc, const Change& change) override {

        if (change.everything || change.volume_change) {
            cell_list.reset(new CellList_T(spc.geometry.getLength(), cell_length));
            for (const auto& particle : spc.activeParticles()) {
                const auto particle_index = indexOf(particle);
                cell_list->insertMember(particle_index, particle.pos + spc.geometry.getLength() / 2.);
            }
        } else if (!change.matter_change) {
            for (const auto& group_change : change.groups) {
                const auto& group = spc.groups.at(group_change.group_index);
                const auto offset = spc.getFirstParticleIndex(group);
                if( group_change.relative_atom_indices.empty() ){
                    auto indices = ranges::cpp20::views::iota(offset, offset + group.size());
                    for( const auto index : indices){
                        cell_list->updateMemberAt(index, spc.particles.at(index).pos + spc.geometry.getLength() / 2.);
                    }
                }
                auto absolute_atom_index = group_change.relative_atom_indices |
                                           ranges::cpp20::views::transform([offset](auto i) { return i + offset; });
                for (auto i : absolute_atom_index) {
                    cell_list->updateMemberAt(i, spc.particles.at(i).pos + spc.geometry.getLength() / 2.);
                }
            }
        } else {
            auto is_active = getGroupFilter<Group::Selectors::ACTIVE>();
            for (const auto& group_change : change.groups) {
                const auto& group = spc.groups.at(group_change.group_index);
                const auto offset = spc.getFirstParticleIndex(group);
                if (is_active(group)) { // if the group is active, there is at least one active particle in it
                    for (auto i : group_change.relative_atom_indices) {
                        if (i > std::distance(group.begin(), group.end()) // if index lies behind last active index
                            && cell_list->containsMember(i + offset)) {
                            cell_list->removeMember(i + offset);
                        } else if( !cell_list->containsMember(i + offset) ) {
                            cell_list->insertMember(i + offset,
                                                    spc.particles.at(i + offset).pos + spc.geometry.getLength() / 2.);
                        }
                    }
                } else { // if everything in the group is inactive remove all members
                    for (auto i : group_change.relative_atom_indices) {
                        if (cell_list->containsMember(i + offset)) {
                            cell_list->removeMember(i + offset);
                        }
                    }
                }
            }
        }
    }
};

} // namespace Faunus

#endif // FAUNUS_SASA_H