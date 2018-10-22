#ifndef RecoSeveralEventsTestSuite_h
#define RecoSeveralEventsTestSuite_h


#include <cxxtest/TestSuite.h>
#include <cxxtest/TangoPrinter.h>
#include <tango.h>
#include <iostream>
#include <thread>

using namespace Tango;
using namespace std;

#define cout cout << "\t"
#define    coutv    if (verbose == true) cout

#undef SUITE_NAME
#define SUITE_NAME RecoSeveralEventsTestSuite

class EventCallback : public Tango::CallBack
{
public:
    EventCallback(string cb_name)  { name = cb_name; };
    ~EventCallback() { };
    void push_event( Tango::EventData *ed )
    {
        cout << "In callback " << name << " with error flag = " << std::boolalpha << ed->err << endl;
        if(ed->err)
        {
            cb_err++;
            cout << "Error: " << ed->errors[0].reason << endl;
        }
        else
        {
            cb_executed++;
        }
    }


    int cb_executed;
    int cb_err;
    string name;
};

class RecoSeveralEventsTestSuite : public CxxTest::TestSuite
{
protected:
    DeviceProxy *device1, *device2;
    DeviceProxy *admin1_dev;
    DeviceProxy *admin2_dev;
    string device1_name, device2_name, device1_instance_name, device2_instance_name;
    string adm1_name, adm2_name;
    bool verbose;
    EventCallback eventCallback1;
    EventCallback eventCallback2;

public:
    SUITE_NAME() :
            device1_instance_name("test"),//TODO pass via cl
            device2_instance_name("test2"),
            eventCallback1("1"),
            eventCallback2("2")
    {
//
// Arguments check -------------------------------------------------
//
        device1_name = CxxTest::TangoPrinter::get_param("device1");
        device2_name = CxxTest::TangoPrinter::get_param("device20");

        verbose = CxxTest::TangoPrinter::is_param_defined("verbose");

        CxxTest::TangoPrinter::validate_args();


//
// Initialization --------------------------------------------------
//
        try
        {
            // Kill DevTest servers to start clean
            CxxTest::TangoPrinter::kill_server();
            // Start server 1 and set fallback point
            CxxTest::TangoPrinter::start_server(device1_instance_name);
            // Start server 2 and set fallback point
            CxxTest::TangoPrinter::start_server(device2_instance_name);

            // Give some time for the servers to start up
            Tango_sleep(3);

            device1 = new DeviceProxy(device1_name);
            device2 = new DeviceProxy(device2_name);

            adm1_name = device1->adm_name();
            adm2_name = device2->adm_name();

            admin1_dev = new DeviceProxy(adm1_name);
            admin2_dev = new DeviceProxy(adm2_name);

            //sleep 18 &&  start_server "@INST_NAME@" &
            thread([this]() {
                Tango_sleep(18);
                CxxTest::TangoPrinter::start_server(device1_instance_name);
                CxxTest::TangoPrinter::start_server(device2_instance_name);
            }).detach();

            //sleep 62 &&  start_server "@INST_NAME@" &
            thread([this]() {
                Tango_sleep(62);
                CxxTest::TangoPrinter::start_server(device1_instance_name);
                CxxTest::TangoPrinter::start_server(device2_instance_name);
            }).detach();
        }
        catch (CORBA::Exception &e)
        {
            Except::print_exception(e);
            exit(-1);
        }

    }

    virtual ~SUITE_NAME()
    {
        // Kill all DevTest instances
        CxxTest::TangoPrinter::kill_server();
        // Restart device 1
        CxxTest::TangoPrinter::start_server(device1_instance_name);

        delete device1;
        delete device2;
        delete admin1_dev;
        delete admin2_dev;
    }

    static SUITE_NAME *createSuite()
    {
        return new SUITE_NAME();
    }

    static void destroySuite(SUITE_NAME *suite)
    {
        delete suite;
    }

//
// Tests -------------------------------------------------------
//

//
// Subscribe to user event on several devices
//
    void test_subscribe_to_user_event_on_several_devices(void)
    {
        string att_name("event_change_tst");

        const vector<string> filters;
        eventCallback1.cb_executed = 0;
        eventCallback1.cb_err = 0;
        eventCallback2.cb_executed = 0;
        eventCallback2.cb_err = 0;

        TS_ASSERT_THROWS_NOTHING(device1->subscribe_event(att_name, Tango::USER_EVENT, &eventCallback1, filters));
        TS_ASSERT_THROWS_NOTHING(device2->subscribe_event(att_name, Tango::USER_EVENT, &eventCallback2, filters));

//
// Fire 2 events on device #1 and 1 event on device #2
//
        TS_ASSERT_THROWS_NOTHING(device1->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device1->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device2->command_inout("IOPushEvent"));

        Tango_sleep(1);

        coutv << "Callback1 execution before re-connection = " << eventCallback1.cb_executed << endl;
        coutv << "Callback1 error before re-connection = " << eventCallback1.cb_err << endl;

        TS_ASSERT_EQUALS (eventCallback1.cb_executed, 3);
        TS_ASSERT_EQUALS (eventCallback1.cb_err, 0);

        coutv << "Callback2 execution before re-connection = " << eventCallback2.cb_executed << endl;
        coutv << "Callback2 error before re-connection = " << eventCallback2.cb_err << endl;

        TS_ASSERT_EQUALS (eventCallback2.cb_executed, 2);
        TS_ASSERT_EQUALS (eventCallback2.cb_err, 0);

//
// Kill device servers (using their admin device)
//

        TS_ASSERT_THROWS_NOTHING(admin1_dev->command_inout("kill"));
        TS_ASSERT_THROWS_NOTHING(admin2_dev->command_inout("kill"));

//
// Wait for some error and re-connection
//

        Tango_sleep(40);

//
// Check error and re-connection
//

        coutv << "Callback1 execution after re-connection = " << eventCallback1.cb_executed << endl;
        coutv << "Callback1 error after re-connection = " << eventCallback1.cb_err << endl;

        TS_ASSERT_LESS_THAN_EQUALS (1, eventCallback1.cb_err);
        TS_ASSERT_EQUALS (eventCallback1.cb_executed, 4);

        coutv << "Callback2 execution after re-connection = " << eventCallback2.cb_executed << endl;
        coutv << "Callback2 error after re-connection = " << eventCallback2.cb_err << endl;

        TS_ASSERT_LESS_THAN_EQUALS (1, eventCallback2.cb_err);
        TS_ASSERT_EQUALS (eventCallback2.cb_executed, 3);

//
// Fire other events
//

        TS_ASSERT_THROWS_NOTHING(device1->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device2->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device2->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device2->command_inout("IOPushEvent"));

        Tango_sleep(1);

        coutv << "Callback1 execution after re-connection and event = " << eventCallback1.cb_executed << endl;
        coutv << "Callback1 error after re-connection and event = " << eventCallback1.cb_err << endl;

        TS_ASSERT_EQUALS (eventCallback1.cb_executed, 5);
        TS_ASSERT_LESS_THAN_EQUALS (1, eventCallback1.cb_err);

        coutv << "Callback2 execution after re-connection and event = " << eventCallback2.cb_executed << endl;
        coutv << "Callback2 error after re-connection and event = " << eventCallback2.cb_err << endl;

        TS_ASSERT_EQUALS (eventCallback2.cb_executed, 6);
        TS_ASSERT_LESS_THAN_EQUALS (1, eventCallback2.cb_err);
    }

//
// Clear call back counters and kill device server once more
//
    void test_clear_cb_kill_ds(void) {
        eventCallback1.cb_executed = 0;
        eventCallback1.cb_err = 0;
        eventCallback2.cb_executed = 0;
        eventCallback2.cb_err = 0;

        TS_ASSERT_THROWS_NOTHING(admin1_dev->command_inout("kill"));
        TS_ASSERT_THROWS_NOTHING(admin2_dev->command_inout("kill"));

//
// Wait for some error and re-connection
//

        Tango_sleep(40);

//
// Check error and re-connection
//

        coutv << "Callback1 execution after second re-connection = " << eventCallback1.cb_executed << endl;
        coutv << "Callback1 error after second re-connection = " << eventCallback1.cb_err << endl;

        TS_ASSERT_LESS_THAN_EQUALS (1, eventCallback1.cb_err);
        TS_ASSERT_EQUALS (eventCallback1.cb_executed, 1);

        coutv << "Callback2 execution after second re-connection = " << eventCallback2.cb_executed << endl;
        coutv << "Callback2 error after second re-connection = " << eventCallback2.cb_err << endl;

        TS_ASSERT_LESS_THAN_EQUALS (1, eventCallback2.cb_err);
        TS_ASSERT_EQUALS (eventCallback2.cb_executed, 1);

//
// Fire other events
//

        TS_ASSERT_THROWS_NOTHING(device1->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device2->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device1->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device2->command_inout("IOPushEvent"));
        TS_ASSERT_THROWS_NOTHING(device2->command_inout("IOPushEvent"));

        Tango_sleep(2);

        coutv << "Callback1 execution after second re-connection and event = " << eventCallback1.cb_executed << endl;
        coutv << "Callback1 error after second re-connection and event = " << eventCallback1.cb_err << endl;

        TS_ASSERT_EQUALS (eventCallback1.cb_executed, 3);
        TS_ASSERT_LESS_THAN_EQUALS (1, eventCallback1.cb_err);

        coutv << "Callback2 execution after second re-connection and event = " << eventCallback2.cb_executed << endl;
        coutv << "Callback2 error after second re-connection and event = " << eventCallback2.cb_err << endl;

        TS_ASSERT_EQUALS (eventCallback2.cb_executed, 4);
        TS_ASSERT_LESS_THAN_EQUALS (1, eventCallback2.cb_err);
    }
};

#undef cout
#endif // RecoSeveralEventsTestSuite_h

