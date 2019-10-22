// Copyright (C) 2019 by Jakub Wojciech

// This file is part of Lelo Remote Music Player.

// Lelo Remote Music Player is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.

// Lelo Remote Music Player is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with Lelo Remote Music Player. If not, see
// <https://www.gnu.org/licenses/>.

#include <gtest/gtest.h>

#include "crypto/certs/Certificate.h"
#include "crypto/certs/CertificateAuthority.h"
#include "crypto/certs/CertificateRequest.h"
#include "crypto/certs/EddsaKeyPair.h"
#include "crypto/certs/RsaKeyPair.h"

#include <openssl/pem.h>

#include "filesystem.h"
#include "Util.h"

using namespace lrm::crypto::certs;

const fs::path serialized_privkey_path = "/tmp/lrm-test-privkey.pem";
const fs::path serialized_pubkey_path = "/tmp/lrm-test-pubkey.pem";
const std::string serialized_cert_path = "/tmp/lrm-test-cert.pem";
std::shared_ptr<EddsaKeyPair> glob_key_pair;

bool check_pem_file_contents(std::string_view filename) {
  std::string line;
  auto fs = std::ifstream(filename.data());
  std::getline(fs, line);
  if (line.find("-----BEGIN ") != 0) return false;

  std::string last_line = line;
  while (std::getline(fs, line)) {
    if (line.empty()) break;
    last_line = line;
  }

  if (last_line.find("-----END ") == 0) return true;
  return false;
}

template <typename T>
class KeyPairTest : public ::testing::Test {
 public:
  static std::unique_ptr<T> key_pair;
};
template <typename T>
std::unique_ptr<T> KeyPairTest<T>::key_pair;

using KeyPairTypes = ::testing::Types<RsaKeyPair, EddsaKeyPair>;
TYPED_TEST_CASE(KeyPairTest, KeyPairTypes);

TYPED_TEST(KeyPairTest, Constructs) {
  EXPECT_NO_THROW(TestFixture::key_pair = std::make_unique<TypeParam>());
}

TYPED_TEST(KeyPairTest, Generates) {
  EXPECT_NO_THROW(TestFixture::key_pair->Generate());
}

TYPED_TEST(KeyPairTest, SerializesPrivkeyToPemFile) {
  fs::remove(serialized_privkey_path);

  EXPECT_NO_THROW(
      TestFixture::key_pair->ToPemFilePrivKey(serialized_privkey_path));
  ASSERT_TRUE(lrm::Util::file_exists(serialized_privkey_path.c_str()));

  EXPECT_TRUE(check_pem_file_contents(serialized_privkey_path.string()));
}

TYPED_TEST(KeyPairTest, SerializesPubkeyToPemFile) {
  fs::remove(serialized_pubkey_path);

  EXPECT_NO_THROW(
      TestFixture::key_pair->ToPemFilePubKey(serialized_pubkey_path));
  ASSERT_TRUE(lrm::Util::file_exists(serialized_pubkey_path.c_str()));

  EXPECT_TRUE(check_pem_file_contents(serialized_pubkey_path.string()));
}

TYPED_TEST(KeyPairTest, ToPemString) {
  std::string key_pair_str;
  ASSERT_NO_THROW(key_pair_str = TestFixture::key_pair->ToPemPrivKey());
  ASSERT_FALSE(key_pair_str.empty());

  EXPECT_EQ(0, key_pair_str.find("-----BEGIN PRIVATE KEY-----"))
      << "The PEM private key string should start with "
      "-----BEGIN PRIVATE KEY-----";
}

TYPED_TEST(KeyPairTest, DeserializeFromFile) {
  std::string key = TestFixture::key_pair->ToPemPrivKey();

  TestFixture::key_pair.reset(new TypeParam);
  ASSERT_NO_THROW(
      TestFixture::key_pair->FromPemFile(serialized_privkey_path));

  EXPECT_EQ(key, TestFixture::key_pair->ToPemPrivKey());
}

// TODO: add password encrypted (de)serialization tests
// TODO: add tests for DER serialization


// -------------------- CERTIFICATE TESTS --------------------

class CertificateTest : public ::testing::Test {
  inline static constexpr auto CA_privkey_pem =
      "-----BEGIN PRIVATE KEY-----\n"
      "MC4CAQAwBQYDK2VwBCIEIHf8jm7hxCwO/3yKihJBZktmg/iA1ma/WEabZf/MDqrN\n"
      "-----END PRIVATE KEY-----\n";

  inline static constexpr auto good_privkey_pem =
      "-----BEGIN PRIVATE KEY-----\n"
      "MC4CAQAwBQYDK2VwBCIEIGb1ageNgHFPC0nJEaTFy4q3+ybg7wW2tX4V5xh7xqoN\n"
      "-----END PRIVATE KEY-----\n";

 protected:
  // inline static const EddsaKeyPair key_pair = []{
  //                                               EddsaKeyPair kp;
  //                                               kp.FromPEM(CA_privkey_pem);
  //                                               return kp;
  //                                             }();
  inline static constexpr auto CA_cert_pem =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIBmzCCAU2gAwIBAgIUHeVgIYnpzLshMqmYEh81ltKSCY0wBQYDK2VwMEMxCzAJ\n"
      "BgNVBAYTAlBMMRIwEAYDVQQIDAlMYXJtb2xhbmQxDjAMBgNVBAoMBUxhcm1vMRAw\n"
      "DgYDVQQDDAdMYXJtb0NOMB4XDTE5MTAyMTExMzkxMloXDTE5MTEyMDExMzkxMlow\n"
      "QzELMAkGA1UEBhMCUEwxEjAQBgNVBAgMCUxhcm1vbGFuZDEOMAwGA1UECgwFTGFy\n"
      "bW8xEDAOBgNVBAMMB0xhcm1vQ04wKjAFBgMrZXADIQCFQAmkVerTerwycGZiuzaK\n"
      "/WAp4xrt34JXM0LNZ2e51aNTMFEwHQYDVR0OBBYEFKOuVXhpBxcBBGEl5X5hgMVY\n"
      "uEzUMB8GA1UdIwQYMBaAFKOuVXhpBxcBBGEl5X5hgMVYuEzUMA8GA1UdEwEB/wQF\n"
      "MAMBAf8wBQYDK2VwA0EAunjCsJ/4P1NEhrNUhPV5gs5jsvWMSrZh4GzlMofoW0v7\n"
      "4+FobSRL4S0CO5FOFf2+tUclpuqTRcvI7RNXqKF1Cg==\n"
      "-----END CERTIFICATE-----\n";

  inline static constexpr auto good_cert_pem =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIBSjCB/QIUJmA+DtNgkD4VxtKbfoJEATsBhKUwBQYDK2VwMEMxCzAJBgNVBAYT\n"
      "AlBMMRIwEAYDVQQIDAlMYXJtb2xhbmQxDjAMBgNVBAoMBUxhcm1vMRAwDgYDVQQD\n"
      "DAdMYXJtb0NOMB4XDTE5MTAyMTE3NDcwOVoXDTE5MTEyMDE3NDcwOVowTTELMAkG\n"
      "A1UEBhMCUEwxFjAUBgNVBAgMDUFsc29MYXJtb2xhbmQxETAPBgNVBAoMCE5vdExh\n"
      "cm1vMRMwEQYDVQQDDApOb3RMYXJtb0NOMCowBQYDK2VwAyEAD4uVdAmFYZzjTKfu\n"
      "1qSqL3fkRTL0GrvTCDLJw4vdqxswBQYDK2VwA0EAmFGqzcmN4IcWseyCKgjZiAd9\n"
      "HMP952zSDQHzGrBkMr1+P3iWTsGnQGyXrmU0RI4o78t2lX8IyrxDKxYvgOGmDQ==\n"
      "-----END CERTIFICATE-----\n";

  inline static const auto cert_pem_file = "test-certs_cert.pem";

  void SetUp() {
    fs::remove(cert_pem_file);
  }
};

TEST_F(CertificateTest, FromPem) {
  EXPECT_NO_THROW(Certificate::FromPem(CA_cert_pem));
}

TEST_F(CertificateTest, ToPem) {
  const auto cert = Certificate::FromPem(CA_cert_pem);

  EXPECT_EQ(cert.ToPem(), CA_cert_pem);
}

TEST_F(CertificateTest, Verify_Self) {
  const auto cert = Certificate::FromPem(CA_cert_pem);

  EXPECT_TRUE(cert.Verify(cert));
}

TEST_F(CertificateTest, Verify_AnotherGood) {
  const auto CAcert = Certificate::FromPem(CA_cert_pem);
  const auto goodCert = Certificate::FromPem(good_cert_pem);

  EXPECT_TRUE(CAcert.Verify(goodCert));
}

TEST_F(CertificateTest, Verify_AnotherBad) {
  const auto CAcert = Certificate::FromPem(good_cert_pem);
  const auto badCert = Certificate::FromPem(CA_cert_pem);

  EXPECT_FALSE(CAcert.Verify(badCert));
}

TEST_F(CertificateTest, GetExtensions) {
  const auto cert = Certificate::FromPem(CA_cert_pem);

  const auto extensions = cert.GetExtensions();

  ASSERT_EQ(3, extensions.size());
  EXPECT_EQ(
      extensions.at("X509v3 Authority Key Identifier"),
      "keyid:A3:AE:55:78:69:07:17:01:04:61:25:E5:7E:61:80:C5:58:B8:4C:D4\n");
  EXPECT_EQ(
      extensions.at("X509v3 Subject Key Identifier"),
      "A3:AE:55:78:69:07:17:01:04:61:25:E5:7E:61:80:C5:58:B8:4C:D4");
  EXPECT_EQ(extensions.at("X509v3 Basic Constraints"), "CA:TRUE");
}

TEST_F(CertificateTest, GetSubjectName) {
  const auto cert = Certificate::FromPem(CA_cert_pem);

  const auto name = cert.GetSubjectName();

  ASSERT_EQ(4, name.size());
  EXPECT_EQ(name.at("organizationName"), "Larmo");
  EXPECT_EQ(name.at("stateOrProvinceName"), "Larmoland");
  EXPECT_EQ(name.at("commonName"), "LarmoCN");
  EXPECT_EQ(name.at("countryName"), "PL");
}

TEST_F(CertificateTest, GetIssuerName_SelfSigned) {
  const auto cert = Certificate::FromPem(CA_cert_pem);

  EXPECT_EQ(cert.GetIssuerName(), cert.GetSubjectName());
}

TEST_F(CertificateTest, ToPemFile) {
  const auto cert = Certificate::FromPem(CA_cert_pem);

  cert.ToPemFile(cert_pem_file);
  ASSERT_TRUE(lrm::Util::file_exists(cert_pem_file));
  EXPECT_TRUE(check_pem_file_contents(cert_pem_file));
}

TEST_F(CertificateTest, FromPemFile) {
  Certificate::FromPem(CA_cert_pem).ToPemFile(cert_pem_file);

  auto cert = Certificate::FromPemFile(cert_pem_file);

  EXPECT_EQ(cert.GetSubjectName().size(), 4);
}

TEST_F(CertificateTest, FromPemFile_DoesntExist) {
  EXPECT_THROW(Certificate::FromPemFile(cert_pem_file),
               std::invalid_argument);
}

TEST_F(CertificateTest, ToDer) {
  auto cert = Certificate::FromPem(CA_cert_pem);

  const auto der = cert.ToDer();
  EXPECT_FALSE(der.empty());
}

TEST_F(CertificateTest, FromDer) {
  const auto control = Certificate::FromPem(CA_cert_pem);

  auto cert = Certificate::FromDer(control.ToDer());

  EXPECT_EQ(control.ToPem(), cert.ToPem());
}

// -------------------- CERTIFICATE REQUEST TESTS --------------------

class CertificateRequestTest : public ::testing::Test {
 protected:
  inline static constexpr auto req_pem =
      "-----BEGIN CERTIFICATE REQUEST-----\n"
      "MIHrMIGeAgEAME0xCzAJBgNVBAYTAlBMMRYwFAYDVQQIDA1BbHNvTGFybW9sYW5k\n"
      "MREwDwYDVQQKDAhOb3RMYXJtbzETMBEGA1UEAwwKTm90TGFybW9DTjAqMAUGAytl\n"
      "cAMhAA+LlXQJhWGc40yn7takqi935EUy9Bq70wgyycOL3asboB4wHAYJKoZIhvcN\n"
      "AQkOMQ8wDTALBgNVHQ8EBAMCAwgwBQYDK2VwA0EAN75vvccnVJBYXn9IrwDdIXO8\n"
      "SeYVrYlTsp4ak6HXp26oXsEtyrIo4KkQNDPcsifpI67naCHx/lbwokfVsyxlDw==\n"
      "-----END CERTIFICATE REQUEST-----\n";

  inline static constexpr auto req_key_pair_pem =
      "-----BEGIN PRIVATE KEY-----\n"
      "MC4CAQAwBQYDK2VwBCIEIGb1ageNgHFPC0nJEaTFy4q3+ybg7wW2tX4V5xh7xqoN\n"
      "-----END PRIVATE KEY-----\n";

  inline static EddsaKeyPair req_key_pair =
      []{
        EddsaKeyPair kp;
        kp.FromPEM(req_key_pair_pem);
        return kp;
      }();
};

TEST_F(CertificateRequestTest, FromPem) {
  auto req = CertificateRequest::FromPem(req_pem);

  EXPECT_NE(nullptr, req.Get());
}

TEST_F(CertificateRequestTest, ToPem) {
  auto req = CertificateRequest::FromPem(req_pem);

  EXPECT_EQ(req.ToPem(), req_pem);
}

TEST_F(CertificateRequestTest, GetName) {
  auto req = CertificateRequest::FromPem(req_pem);

  const auto name = req.GetName();

  ASSERT_EQ(4, name.size());
  EXPECT_EQ("PL", name.at("countryName"));
  EXPECT_EQ("AlsoLarmoland", name.at("stateOrProvinceName"));
  EXPECT_EQ("NotLarmo", name.at("organizationName"));
  EXPECT_EQ("NotLarmoCN", name.at("commonName"));
}

TEST_F(CertificateRequestTest, GetExtensions) {
  auto req = CertificateRequest::FromPem(req_pem);

  const auto extensions = req.GetExtensions();

  ASSERT_EQ(1, extensions.size());

  EXPECT_EQ("Key Agreement", extensions.at("X509v3 Key Usage"));
}

TEST_F(CertificateRequestTest, Construct) {
  auto req = CertificateRequest{
    req_key_pair,
    {{"countryName", "RE"},
     {"stateOrProvinceName", "ReqProvince"},
     {"organizationName", "ReqOrganization"},
     {"commonName", "ReqCN"}}
  };

  ASSERT_NE(req.Get(), nullptr);

  const auto name = req.GetName();
  ASSERT_EQ(4, name.size());
  EXPECT_EQ(name.at("organizationName"), "ReqOrganization");
  EXPECT_EQ(name.at("stateOrProvinceName"), "ReqProvince");
  EXPECT_EQ(name.at("commonName"), "ReqCN");
  EXPECT_EQ(name.at("countryName"), "RE");
}

// ------------------ CERTIFICATE AUTHORITY TESTS ------------------

class CertificateAuthorityTest : public ::testing::Test {
 protected:
  inline static const auto CA_key_pair =
      []{
        auto kp = std::make_shared<EddsaKeyPair>();
        kp->FromPEM("-----BEGIN PRIVATE KEY-----\n"
                   "MC4CAQAwBQYDK2VwBCIEIHf8jm7hxCwO/"
                   "3yKihJBZktmg/iA1ma/WEabZf/MDqrN\n"
                   "-----END PRIVATE KEY-----\n");
        return kp;
      }();

  inline static const Map CA_name = {{"organizationName", "Larmo"},
                                     {"stateOrProvinceName", "Larmoland"},
                                     {"commonName", "LarmoCN"},
                                     {"countryName", "PL"}};

  inline static constexpr auto good_privkey_pem =
      "-----BEGIN PRIVATE KEY-----\n"
      "MC4CAQAwBQYDK2VwBCIEIGb1ageNgHFPC0nJEaTFy4q3+ybg7wW2tX4V5xh7xqoN\n"
      "-----END PRIVATE KEY-----\n";

  static void SetUpTestCase() {

  }
};

TEST_F(CertificateAuthorityTest, Construct) {
  const auto CA = CertificateAuthority{CA_name, CA_key_pair};

  const auto name = CA.GetRootCertificate().GetIssuerName();
  ASSERT_FALSE(name.empty());
  EXPECT_EQ(CA_name, name);
  EXPECT_EQ(CA.GetRootCertificate().GetSubjectName(), name);

  // TODO: enable this part of the test
  // const auto extensions = CA.GetRootCertificate().GetExtensions();
  // ASSERT_FALSE(extensions.empty());
  // EXPECT_EQ("CA:TRUE", extensions.at("X509v3 Basic Constraints"));
}

TEST_F(CertificateAuthorityTest, Certify) {
  EddsaKeyPair kp; kp.FromPEM(good_privkey_pem);
  Map name = {{"countryName", "PL"},
              {"stateOrProvinceName", "AlsoLarmoland"},
              {"organizationName", "NotLarmo"},
              {"commonName", "NotLarmoCN"}};

  CertificateRequest request{kp, name};
  auto CA = CertificateAuthority{CA_name, CA_key_pair};

  Certificate cert = CA.Certify(std::move(request), 365);

  auto subject_name = cert.GetSubjectName();
  EXPECT_EQ(name, subject_name);
  EXPECT_EQ(CA.GetRootCertificate().GetSubjectName(), cert.GetIssuerName());
  EXPECT_TRUE(CA.GetRootCertificate().Verify(cert));
}
