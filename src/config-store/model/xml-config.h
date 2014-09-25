#ifndef XML_CONFIG_STORE_H
#define XML_CONFIG_STORE_H

#include <string>
#ifdef HAVE_LIBXML2
#include <libxml/xmlwriter.h>
#include <libxml/xmlreader.h>
#endif
#include "file-config.h"

namespace ns3 {

/**
 * \ingroup configstore
 *
 */
class XmlConfigSave : public FileConfig
{
public:
  XmlConfigSave ();
  virtual ~XmlConfigSave ();

  virtual void SetFilename (std::string filename);
  virtual void Default (void);
  virtual void Global (void);
  virtual void Attributes (void);
private:
#ifdef HAVE_LIBXML2
  xmlTextWriterPtr m_writer;
#endif
};

/**
 * \ingroup configstore
 *
 */
class XmlConfigLoad : public FileConfig
{
public:
  XmlConfigLoad ();
  virtual ~XmlConfigLoad ();

  virtual void SetFilename (std::string filename);
  virtual void Default (void);
  virtual void Global (void);
  virtual void Attributes (void);
private:
  std::string m_filename;
};

} // namespace ns3

#endif /* XML_CONFIG_STORE_H */
