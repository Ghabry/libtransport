#include "transport/userregistry.h"
#include "transport/config.h"
#include "transport/storagebackend.h"
#include "transport/user.h"
#include "transport/transport.h"
#include "transport/conversation.h"
#include "transport/usermanager.h"
#include "transport/localbuddy.h"
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <Swiften/Swiften.h>
#include <Swiften/EventLoop/DummyEventLoop.h>
#include <Swiften/Server/Server.h>
#include <Swiften/Network/DummyNetworkFactories.h>
#include <Swiften/Network/DummyConnectionServer.h>
#include "Swiften/Server/ServerStanzaChannel.h"
#include "Swiften/Server/ServerFromClientSession.h"
#include "Swiften/Parser/PayloadParsers/FullPayloadParserFactoryCollection.h"
#include "basictest.h"

using namespace Transport;

class RosterManagerTest : public CPPUNIT_NS :: TestFixture, public BasicTest {
	CPPUNIT_TEST_SUITE(RosterManagerTest);
	CPPUNIT_TEST(setBuddy);
	CPPUNIT_TEST(sendCurrentPresences);
	CPPUNIT_TEST(sendCurrentPresence);
	CPPUNIT_TEST(removeBuddy);
	CPPUNIT_TEST(subscribeExistingBuddy);
	CPPUNIT_TEST(subscribeNewBuddy);
	CPPUNIT_TEST(unsubscribeExistingBuddy);
	CPPUNIT_TEST(unsubscribeNewBuddy);
	CPPUNIT_TEST_SUITE_END();

	public:
		std::string m_buddy;

		void setUp (void) {
			m_buddy.clear();
			setMeUp();
			connectUser();
			User *user = userManager->getUser("user@localhost");
			user->getRosterManager()->onBuddyAdded.connect(boost::bind(&RosterManagerTest::handleBuddyAdded, this, _1));
			user->getRosterManager()->onBuddyRemoved.connect(boost::bind(&RosterManagerTest::handleBuddyRemoved, this, _1));
			received.clear();
		}

		void tearDown (void) {
			received.clear();
			disconnectUser();
			tearMeDown();
		}

	void handleBuddyAdded(Buddy *buddy) {
		m_buddy = buddy->getName();
	}

	void handleBuddyRemoved(Buddy *buddy) {
		m_buddy = buddy->getName();
	}

	void connectUser() {
		CPPUNIT_ASSERT_EQUAL(0, userManager->getUserCount());
		userRegistry->isValidUserPassword(Swift::JID("user@localhost/resource"), serverFromClientSession.get(), Swift::createSafeByteArray("password"));
		loop->processEvents();
		CPPUNIT_ASSERT_EQUAL(1, userManager->getUserCount());

		User *user = userManager->getUser("user@localhost");
		CPPUNIT_ASSERT(user);

		UserInfo userInfo = user->getUserInfo();
		CPPUNIT_ASSERT_EQUAL(std::string("password"), userInfo.password);
		CPPUNIT_ASSERT(user->isReadyToConnect() == true);
		CPPUNIT_ASSERT(user->isConnected() == false);

		user->setConnected(true);
		CPPUNIT_ASSERT(user->isConnected() == true);

		CPPUNIT_ASSERT_EQUAL(1, (int) received.size());
		CPPUNIT_ASSERT(getStanza(received[0])->getPayload<Swift::DiscoInfo>());
		received.clear();
	}

	void add2Buddies() {
		User *user = userManager->getUser("user@localhost");
		CPPUNIT_ASSERT(user);

		std::vector<std::string> grp;
		grp.push_back("group1");
		LocalBuddy *buddy = new LocalBuddy(user->getRosterManager(), -1, "buddy1", "Buddy 1", grp, BUDDY_JID_ESCAPING);
		buddy->setStatus(Swift::StatusShow(Swift::StatusShow::Away), "status1");
		user->getRosterManager()->setBuddy(buddy);

		std::vector<std::string> grp2;
		grp2.push_back("group2");
		buddy = new LocalBuddy(user->getRosterManager(), -1, "buddy2", "Buddy 2", grp2, BUDDY_JID_ESCAPING);
		buddy->setStatus(Swift::StatusShow(Swift::StatusShow::Away), "status2");
		user->getRosterManager()->setBuddy(buddy);
	}

	void setBuddy() {
		add2Buddies();
		CPPUNIT_ASSERT_EQUAL(2, (int) received.size());

		Swift::RosterPayload::ref payload1 = getStanza(received[0])->getPayload<Swift::RosterPayload>();
		CPPUNIT_ASSERT(payload1);
		CPPUNIT_ASSERT_EQUAL(1, (int) payload1->getItems().size());
		Swift::RosterItemPayload item = payload1->getItems()[0];
		CPPUNIT_ASSERT_EQUAL(std::string("buddy1"), Buddy::JIDToLegacyName(item.getJID()));
		CPPUNIT_ASSERT_EQUAL(std::string("Buddy 1"), item.getName());

		Swift::RosterPayload::ref payload2 = getStanza(received[1])->getPayload<Swift::RosterPayload>();
		CPPUNIT_ASSERT(payload2);
		CPPUNIT_ASSERT_EQUAL(1, (int) payload2->getItems().size());
		item = payload2->getItems()[0];
		CPPUNIT_ASSERT_EQUAL(std::string("buddy2"), Buddy::JIDToLegacyName(item.getJID()));
		CPPUNIT_ASSERT_EQUAL(std::string("Buddy 2"), item.getName());

		// send responses back
		injectIQ(Swift::IQ::createResult(getStanza(received[0])->getFrom(), getStanza(received[0])->getTo(), getStanza(received[0])->getID()));
		injectIQ(Swift::IQ::createResult(getStanza(received[1])->getFrom(), getStanza(received[1])->getTo(), getStanza(received[1])->getID()));

		// we should get presences
		CPPUNIT_ASSERT_EQUAL(4, (int) received.size());
		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[2])));
		CPPUNIT_ASSERT_EQUAL(Swift::StatusShow::Away, dynamic_cast<Swift::Presence *>(getStanza(received[2]))->getShow());
		CPPUNIT_ASSERT_EQUAL(std::string("status1"), dynamic_cast<Swift::Presence *>(getStanza(received[2]))->getStatus());

		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[3])));
		CPPUNIT_ASSERT_EQUAL(Swift::StatusShow::Away, dynamic_cast<Swift::Presence *>(getStanza(received[3]))->getShow());
		CPPUNIT_ASSERT_EQUAL(std::string("status2"), dynamic_cast<Swift::Presence *>(getStanza(received[3]))->getStatus());
	}

	void sendCurrentPresences() {
		setBuddy();
		received.clear();

		User *user = userManager->getUser("user@localhost");
		user->getRosterManager()->sendCurrentPresences("user@localhost/resource");

		CPPUNIT_ASSERT_EQUAL(2, (int) received.size());

		for (int i = 0; i < 2; i++) {
			CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[i])));
			CPPUNIT_ASSERT_EQUAL(Swift::StatusShow::Away, dynamic_cast<Swift::Presence *>(getStanza(received[i]))->getShow());
			CPPUNIT_ASSERT_EQUAL(std::string("user@localhost/resource"), dynamic_cast<Swift::Presence *>(getStanza(received[i]))->getTo().toString());
		}
	}

	void sendCurrentPresence() {
		setBuddy();
		received.clear();

		User *user = userManager->getUser("user@localhost");
		user->getRosterManager()->sendCurrentPresence("buddy1@localhost", "user@localhost/resource");

		CPPUNIT_ASSERT_EQUAL(1, (int) received.size());
		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[0])));
		CPPUNIT_ASSERT_EQUAL(Swift::StatusShow::Away, dynamic_cast<Swift::Presence *>(getStanza(received[0]))->getShow());
		CPPUNIT_ASSERT_EQUAL(std::string("user@localhost/resource"), dynamic_cast<Swift::Presence *>(getStanza(received[0]))->getTo().toString());

		received.clear();
		user->getRosterManager()->sendCurrentPresence("buddy_unknown@localhost", "user@localhost/resource");
		CPPUNIT_ASSERT_EQUAL(1, (int) received.size());
		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[0])));
		CPPUNIT_ASSERT_EQUAL(Swift::Presence::Unavailable, dynamic_cast<Swift::Presence *>(getStanza(received[0]))->getType());
		CPPUNIT_ASSERT_EQUAL(std::string("user@localhost/resource"), dynamic_cast<Swift::Presence *>(getStanza(received[0]))->getTo().toString());
	}

	void removeBuddy() {
		add2Buddies();
		CPPUNIT_ASSERT_EQUAL(2, (int) received.size());

		received.clear();
		User *user = userManager->getUser("user@localhost");
		user->getRosterManager()->removeBuddy("buddy1");
		CPPUNIT_ASSERT_EQUAL(1, (int) received.size());

		Swift::RosterPayload::ref payload1 = getStanza(received[0])->getPayload<Swift::RosterPayload>();
		CPPUNIT_ASSERT(payload1);
		CPPUNIT_ASSERT_EQUAL(1, (int) payload1->getItems().size());
		Swift::RosterItemPayload item = payload1->getItems()[0];
		CPPUNIT_ASSERT_EQUAL(std::string("buddy1"), Buddy::JIDToLegacyName(item.getJID()));
		CPPUNIT_ASSERT_EQUAL(Swift::RosterItemPayload::Remove, item.getSubscription());
	}

	void subscribeExistingBuddy() {
		add2Buddies();
		received.clear();

		Swift::Presence::ref response = Swift::Presence::create();
		response->setTo("buddy1@localhost");
		response->setFrom("user@localhost/resource");
		response->setType(Swift::Presence::Subscribe);

		injectPresence(response);
		loop->processEvents();

		CPPUNIT_ASSERT_EQUAL(1, (int) received.size());
		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[0])));
		CPPUNIT_ASSERT_EQUAL(Swift::Presence::Subscribed, dynamic_cast<Swift::Presence *>(getStanza(received[0]))->getType());
		CPPUNIT_ASSERT_EQUAL(std::string("buddy1"), m_buddy);
	}

	void subscribeNewBuddy() {
		Swift::Presence::ref response = Swift::Presence::create();
		response->setTo("buddy1@localhost");
		response->setFrom("user@localhost/resource");
		response->setType(Swift::Presence::Subscribe);

		injectPresence(response);
		loop->processEvents();

		CPPUNIT_ASSERT_EQUAL(2, (int) received.size());

		Swift::RosterPayload::ref payload1 = getStanza(received[0])->getPayload<Swift::RosterPayload>();
		CPPUNIT_ASSERT_EQUAL(1, (int) payload1->getItems().size());
		Swift::RosterItemPayload item = payload1->getItems()[0];
		CPPUNIT_ASSERT_EQUAL(std::string("buddy1"), Buddy::JIDToLegacyName(item.getJID()));

		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[1])));
		CPPUNIT_ASSERT_EQUAL(Swift::Presence::Subscribed, dynamic_cast<Swift::Presence *>(getStanza(received[1]))->getType());

		CPPUNIT_ASSERT_EQUAL(std::string("buddy1"), m_buddy);
	}

	void unsubscribeExistingBuddy() {
		add2Buddies();
		received.clear();

		Swift::Presence::ref response = Swift::Presence::create();
		response->setTo("buddy1@localhost");
		response->setFrom("user@localhost/resource");
		response->setType(Swift::Presence::Unsubscribe);

		injectPresence(response);
		loop->processEvents();

		CPPUNIT_ASSERT_EQUAL(2, (int) received.size());

		Swift::RosterPayload::ref payload1 = getStanza(received[0])->getPayload<Swift::RosterPayload>();
		CPPUNIT_ASSERT_EQUAL(1, (int) payload1->getItems().size());
		Swift::RosterItemPayload item = payload1->getItems()[0];
		CPPUNIT_ASSERT_EQUAL(std::string("buddy1"), Buddy::JIDToLegacyName(item.getJID()));


		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[1])));
		CPPUNIT_ASSERT_EQUAL(Swift::Presence::Unsubscribed, dynamic_cast<Swift::Presence *>(getStanza(received[1]))->getType());

		CPPUNIT_ASSERT_EQUAL(std::string("buddy1"), m_buddy);
	}

	void unsubscribeNewBuddy() {
		Swift::Presence::ref response = Swift::Presence::create();
		response->setTo("buddy1@localhost");
		response->setFrom("user@localhost/resource");
		response->setType(Swift::Presence::Unsubscribe);

		injectPresence(response);
		loop->processEvents();

		CPPUNIT_ASSERT_EQUAL(1, (int) received.size());

		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[0])));
		CPPUNIT_ASSERT_EQUAL(Swift::Presence::Unsubscribed, dynamic_cast<Swift::Presence *>(getStanza(received[0]))->getType());

		CPPUNIT_ASSERT_EQUAL(std::string("buddy1"), m_buddy);
	}

	void disconnectUser() {
		userManager->disconnectUser("user@localhost");
		dynamic_cast<Swift::DummyTimerFactory *>(factories->getTimerFactory())->setTime(10);
		loop->processEvents();

		CPPUNIT_ASSERT_EQUAL(0, userManager->getUserCount());
		CPPUNIT_ASSERT_EQUAL(1, (int) received.size());
		CPPUNIT_ASSERT(dynamic_cast<Swift::Presence *>(getStanza(received[0])));
	}

};

CPPUNIT_TEST_SUITE_REGISTRATION (RosterManagerTest);
