
#include "MFTAgeBasedStrategy.h"

#include <sstream>

#include "Population/Person.h"
#include "Therapies/Therapy.hxx"

MFTAgeBasedStrategy::MFTAgeBasedStrategy()
    : IStrategy("MFTAgeBased", MFTAgeBased) {}

void MFTAgeBasedStrategy::add_therapy(Therapy* therapy) {
  therapy_list.push_back(therapy);
}

void MFTAgeBasedStrategy::build_map_age_to_therapy_index() {
  map_age_to_therapy_index.clear();

  constexpr int max_age = 100;
  auto age_boundaries_index = 0;

  for (int age = 0; age < max_age; age++) {
    while (age_boundaries_index < age_boundaries.size()
           && age >= age_boundaries[age_boundaries_index]) {
      age_boundaries_index++;
    }

    auto therapy_index = age_boundaries_index;
    map_age_to_therapy_index.push_back(therapy_index);
  }
}

Therapy* MFTAgeBasedStrategy::get_therapy(Person* person) {
  if (person->age() >= age_boundaries.back()) { return therapy_list.back(); }
  return therapy_list[map_age_to_therapy_index[person->age()]];
}

std::string MFTAgeBasedStrategy::to_string() const {
  std::stringstream sstm;
  sstm << IStrategy::id() << "-" << IStrategy::name() << "-";
  std::string sep;
  for (auto* therapy : therapy_list) {
    sstm << sep << therapy->id();
    sep = ",";
  }
  sep = "";
  sstm << "-";
  for (auto dist : age_boundaries) {
    sstm << sep << dist;
    sep = ",";
  }
  return sstm.str();
}
