/*
 * File:   MACTherapy.h
 * Author: Merlin
 *
 * Created on November 4, 2014, 9:53 AM
 */

#ifndef MACTHERAPY_H
#define MACTHERAPY_H

#include <vector>

#include "Core/PropertyMacro.h"
#include "Therapies/Therapy.hxx"

class MACTherapy : public Therapy {
  DELETE_COPY_AND_MOVE(MACTherapy)

  VIRTUAL_PROPERTY_REF(std::vector<int>, therapy_ids)

  VIRTUAL_PROPERTY_REF(std::vector<int>, start_at_days)

public:
  MACTherapy();

  ~MACTherapy() override;

  void add_therapy_id(const int &therapy_id);

  void add_schedule(const int &start_at_day);

  void print(std::ostream &os) const override;
};

#endif
