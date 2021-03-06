/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2016,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NFD_TESTS_TEST_COMMON_HPP
#define NFD_TESTS_TEST_COMMON_HPP

#include "boost-test.hpp"

#include "core/global-io.hpp"
#include "core/logger.hpp"

#include <ndn-cxx/util/time-unit-test-clock.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#ifdef HAVE_PRIVILEGE_DROP_AND_ELEVATE
#include <unistd.h>
#define SKIP_IF_NOT_SUPERUSER() \
  do { \
    if (::geteuid() != 0) { \
      BOOST_TEST_MESSAGE("This test case needs to be run as superuser, skipping"); \
      return; \
    } \
  } while (false)
#else
#define SKIP_IF_NOT_SUPERUSER()
#endif // HAVE_PRIVILEGE_DROP_AND_ELEVATE

namespace nfd {
namespace tests {

/** \brief base test fixture
 *
 *  Every test case should be based on this fixture,
 *  to have per test case io_service initialization.
 */
class BaseFixture
{
protected:
  BaseFixture()
    : g_io(getGlobalIoService())
  {
  }

  ~BaseFixture()
  {
    resetGlobalIoService();
  }

protected:
  /// reference to global io_service
  boost::asio::io_service& g_io;
};

/** \brief a base test fixture that overrides steady clock and system clock
 */
class UnitTestTimeFixture : public virtual BaseFixture
{
protected:
  UnitTestTimeFixture()
    : steadyClock(make_shared<time::UnitTestSteadyClock>())
    , systemClock(make_shared<time::UnitTestSystemClock>())
  {
    time::setCustomClocks(steadyClock, systemClock);
  }

  ~UnitTestTimeFixture()
  {
    time::setCustomClocks(nullptr, nullptr);
  }

  /** \brief advance steady and system clocks
   *
   *  Clocks are advanced in increments of \p tick for \p nTicks ticks.
   *  After each tick, global io_service is polled to process pending I/O events.
   *
   *  Exceptions thrown during I/O events are propagated to the caller.
   *  Clock advancing would stop in case of an exception.
   */
  void
  advanceClocks(const time::nanoseconds& tick, size_t nTicks = 1)
  {
    this->advanceClocks(tick, tick * nTicks);
  }

  /** \brief advance steady and system clocks
   *
   *  Clocks are advanced in increments of \p tick for \p total time.
   *  The last increment might be shorter than \p tick.
   *  After each tick, global io_service is polled to process pending I/O events.
   *
   *  Exceptions thrown during I/O events are propagated to the caller.
   *  Clock advancing would stop in case of an exception.
   */
  void
  advanceClocks(const time::nanoseconds& tick, const time::nanoseconds& total)
  {
    BOOST_ASSERT(tick > time::nanoseconds::zero());
    BOOST_ASSERT(total >= time::nanoseconds::zero());

    time::nanoseconds remaining = total;
    while (remaining > time::nanoseconds::zero()) {
      if (remaining >= tick) {
        steadyClock->advance(tick);
        systemClock->advance(tick);
        remaining -= tick;
      }
      else {
        steadyClock->advance(remaining);
        systemClock->advance(remaining);
        remaining = time::nanoseconds::zero();
      }

      if (g_io.stopped())
        g_io.reset();
      g_io.poll();
    }
  }

  friend class LimitedIo;

protected:
  shared_ptr<time::UnitTestSteadyClock> steadyClock;
  shared_ptr<time::UnitTestSystemClock> systemClock;
};

inline shared_ptr<Interest>
makeInterest(const Name& name, uint32_t nonce = 0)
{
  auto interest = make_shared<Interest>(name);
  if (nonce != 0) {
    interest->setNonce(nonce);
  }
  return interest;
}

inline shared_ptr<Data>
signData(shared_ptr<Data> data)
{
  ndn::SignatureSha256WithRsa fakeSignature;
  fakeSignature.setValue(ndn::encoding::makeEmptyBlock(tlv::SignatureValue));
  data->setSignature(fakeSignature);
  data->wireEncode();

  return data;
}

inline shared_ptr<Data>
makeData(const Name& name)
{
  auto data = make_shared<Data>(name);
  return signData(data);
}

inline shared_ptr<Link>
makeLink(const Name& name, std::initializer_list<std::pair<uint32_t, Name>> delegations)
{
  auto link = make_shared<Link>(name, delegations);
  signData(link);
  return link;
}

inline lp::Nack
makeNack(const Name& name, uint32_t nonce, lp::NackReason reason)
{
  Interest interest(name);
  interest.setNonce(nonce);
  lp::Nack nack(std::move(interest));
  nack.setReason(reason);
  return nack;
}

} // namespace tests
} // namespace nfd

#endif // NFD_TESTS_TEST_COMMON_HPP
