/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "ZeroconfWIN.h"

#include <string>
#include <sstream>
#include <threads/SingleLock.h>
#include <utils/log.h>

#pragma comment(lib, "dnssd.lib")

CZeroconfWIN::CZeroconfWIN()
{
}

CZeroconfWIN::~CZeroconfWIN()
{
  doStop();
}


//methods to implement for concrete implementations
bool CZeroconfWIN::doPublishService(const std::string& fcr_identifier,
                      const std::string& fcr_type,
                      const std::string& fcr_name,
                      unsigned int f_port,
                      std::map<std::string, std::string> txt)
{
  DNSServiceRef netService = NULL;
  TXTRecordRef txtRecord;
  TXTRecordCreate(&txtRecord, 0, NULL);

  CLog::Log(LOGDEBUG, "CZeroconfWIN::doPublishService identifier: %s type: %s name:%s port:%i", fcr_identifier.c_str(), fcr_type.c_str(), fcr_name.c_str(), f_port);

  //add txt records
  if(!txt.empty())
  {
    for(std::map<std::string, std::string>::const_iterator it = txt.begin(); it != txt.end(); ++it)
    {
      CLog::Log(LOGDEBUG, "CZeroconfWIN: key:%s, value:%s",it->first.c_str(),it->second.c_str());
      uint8_t txtLen = (uint8_t)strlen(it->second.c_str());
      TXTRecordSetValue(&txtRecord, it->first.c_str(), txtLen, it->second.c_str());
    }
  }

  DNSServiceErrorType err = DNSServiceRegister(&netService, 0, 0, assemblePublishedName(fcr_name).c_str(), fcr_type.c_str(), NULL, NULL, htons(f_port), TXTRecordGetLength(&txtRecord), TXTRecordGetBytesPtr(&txtRecord), registerCallback, NULL);

  if(err != kDNSServiceErr_ServiceNotRunning)
    DNSServiceProcessResult(netService);
  
  if (err != kDNSServiceErr_NoError)
  {
    // Something went wrong so lets clean up.
    if (netService)
      DNSServiceRefDeallocate(netService);

    CLog::Log(LOGERROR, "CZeroconfWIN::doPublishService CFNetServiceRegister returned (error = %ld)\n", (int) err);
    if(err == kDNSServiceErr_ServiceNotRunning)
      CLog::Log(LOGERROR, "CZeroconfWIN: Apples Bonjour Service not installed?");
  } 
  else
  {
    CSingleLock lock(m_data_guard);
    m_services.insert(make_pair(fcr_identifier, netService));
  }

  TXTRecordDeallocate(&txtRecord);

  return err == kDNSServiceErr_NoError;
}

bool CZeroconfWIN::doRemoveService(const std::string& fcr_ident)
{
  CSingleLock lock(m_data_guard);
  tServiceMap::iterator it = m_services.find(fcr_ident);
  if(it != m_services.end())
  {
    DNSServiceRefDeallocate(it->second);
    m_services.erase(it);
    return true;
  } 
  else
    return false;
}

void CZeroconfWIN::doStop()
{
  CSingleLock lock(m_data_guard);
  for(tServiceMap::iterator it = m_services.begin(); it != m_services.end(); ++it)
    DNSServiceRefDeallocate(it->second);
  m_services.clear();
}

void DNSSD_API CZeroconfWIN::registerCallback(DNSServiceRef sdref, const DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, void *context)
{
  (void)sdref;    // Unused
  (void)flags;    // Unused
  (void)context;  // Unused

  if (errorCode == kDNSServiceErr_NoError)
  {
    if (flags & kDNSServiceFlagsAdd)
      CLog::Log(LOGDEBUG, "CZeroconfWIN: %s.%s%s now registered and active", name, regtype, domain);
    else
      CLog::Log(LOGDEBUG, "CZeroconfWIN: %s.%s%s registration removed", name, regtype, domain);
  }
  else if (errorCode == kDNSServiceErr_NameConflict)
     CLog::Log(LOGDEBUG, "CZeroconfWIN: %s.%s%s Name in use, please choose another", name, regtype, domain);
  else
    CLog::Log(LOGDEBUG, "CZeroconfWIN: %s.%s%s error code %d", name, regtype, domain, errorCode);

}


std::string CZeroconfWIN::assemblePublishedName(const std::string& fcr_given_name)
{
  std::stringstream ss;
  ss << fcr_given_name << '@';

  // get our hostname
  char lp_hostname[256];
  if (gethostname(lp_hostname, sizeof(lp_hostname)))
  {
    //TODO
    CLog::Log(LOGERROR, "CZeroconfWIN::assemblePublishedName: could not get hostname.. hm... waaaah! PANIC!");
    ss << "DummyThatCantResolveItsName";
  }
  else
  {
    ss << lp_hostname;
  }
  return ss.str();
}