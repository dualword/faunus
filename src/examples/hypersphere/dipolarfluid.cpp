#ifndef HYPERSPHERE
#define HYPERSPHERE
#endif

/*
*/
#include "faunus/faunus.h"
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

using namespace Faunus;
using namespace std;

class hyperrdf : public FAUrdf {
  private:
    double R;
  public:
    hyperrdf(short species1, short species2, float res, hypersphere &con) : FAUrdf(species1, species2, res, acos(-1.)*con.r) {
      R=con.r;
    }
    float get(float x) {
      float simsize=R*R*R*acos(-1.)*acos(-1.)*2.0,
            fact=2*std::acos(-1.)*R*R*R,
            volnew=((x+xres)/R-std::sin((x+xres)*2./R)/2.),
            volold=(x/R-std::sin(x*2./R)/2.);
      return histogram::get(x) * simsize / (volnew-volold) / fact;
    }
};

int main() {
  cout << faunus_splash();
  slump slump;                            // A random number generator
  inputfile in("dipolarfluid.conf");      // Read input file
  hypersphere con(in);                    // We want a hypersphere
  canonical nvt;                          // Use the canonical ensemble
  interaction<pot_hypersphere> pot(in);   // 
  mcloop loop(in);                        // Keep track of time and MC loop
  saltmove sm(nvt,con,pot);               // Salt displacement class
  sm.dp=0.2;                              // Displacement paramter

  hyperrdf rdf_catan(atom["NA"].id, atom["CL"].id, .5, con),
           rdf_anan(atom["CL"].id, atom["CL"].id, .5, con),
           rdf_catcat(atom["NA"].id, atom["NA"].id, .5, con);
 
  macrorot mr(nvt, con, pot);            // Class for molecular rotations
  translate mt(nvt, con, pot);           // Class for molecular translations
  hypermolecule mol;                     // Some molecule on the hypersphere
  vector<particle> spcmodel;             // ...to be loaded with a single water molecule
  mol.add(con, spcmodel, true);          // add a single molecule to the sphere
  
  salt salt;                             // Group for dipoles
  salt.add(con,in);                      // Insert salt read from inputfile
  widom wid(10);                         // Widom particle insertion
  wid.add(con);                          // Determine widom particles from what's in the container
  systemenergy sys(pot.energy(con.p));   // Track system energy
  
  cout << atom.info() << in.info() << pot.info();
  
  while (loop.macroCnt() ) {//Markov chain 
    while (loop.microCnt() ) {
      // mc chain
      switch (rand() % 1) {
        case 0:
          sys += sm.move(salt);
          break;
        case 1:
          sys += mr.move(mol);
          break;
        case 2:
          sys += mt.move(mol);
          break;
      }
      // analysis
      if (slp.random_one()>0.5) {
        rdf_anan.update(con);
        rdf_catan.update(con);
        rdf_catcat.update(con);
        wid.insert(con, pot);        
      }
    } // End of inner loop
    sys.update(pot.energy(con.p));     // Update system energy
    cout << loop.timing();
  } // End of outer loop

  rdf_anan.write("rdf_anan.dat");
  rdf_catan.write("rdf_catan.dat");
  rdf_catcat.write("rdf_catcat.dat");
  
  cout << sys.info() << sm.info() << wid.info();
}

