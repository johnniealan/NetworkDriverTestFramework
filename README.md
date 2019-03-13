# NetworkDriverTestFramework
Generic Method to test Network drivers like UART, USB, CAN, 



Section-1.1: Files required for testing using test framework and to view the test report
=============================================================================================
	tframework				        ->			Latest cross compiled test framework binary
	test_slave.c 					->			Includes test cases & test suites
	test_master.c					->			Includes test cases & test suites
	test_standalone.c				->			Includes test cases & test suites
	test_struct.h					->			Header file for test.c 
	test_framework.xsl				->			XML Style Sheet (req. to view test report)
---------------------------------------------------------------------------------------------
Note: Files are updated in the TFS
---------------------------------------------------------------------------------------------

Section-1.2: Test framework operation
=============================================================================================
Test framework can work in 3 modes:
	1. Slave
	2. Master
	3. Standalone
	
For testing any network driver the process is as follows:
---------------------------------------------------------
	1. Require two systems(one will run as master and other as slave).
	   Slave will be the device under test and master will be the supporting device.
	
	2. Connect two systems over ethernet. 
	   (Primary interface, mandatory to test network dricer)
	
	3. Connect two systems over the interface which need to be tested.
	4. Load the required device drivers.
	5. Generate the required shared objects. (Follow Section-1.3)
	6. Execute the test framework. (Follow Section-1.4)
	
For testing any standalone application/device driver the process is as follows:
-------------------------------------------------------------------------------
	1. Require one system.
	2. Load the required device driver.
	3. Generate the required shared objects. (Follow Section-1.3)
	4. Execute the test framework. (Follow Section-1.4)
---------------------------------------------------------------------------------------------

Section-1.3: Steps for generating shared object of test cases file
=============================================================================================
	1. Create test cases files for the master/slave/standalone mode of operation 
   (Take a reference of test.c, don't forget to include test_struct.h header file)
   
	2. Generate shared objects as shown below:
	
		i. For Slave Mode
		-----------------
		test_slave.c -> File containing test cases
	
		arm-linux-gnueabihf-gcc -c -fPIC test_slave.c -o test_slave.o

		arm-linux-gnueabihf-gcc test_slave.o -shared -o testcases_slave.so
		-----------------
	
		ii. For Master Mode
		-------------------
		test_master.c -> File containing test cases
	
		arm-linux-gnueabihf-gcc -c -fPIC test_master.c -o test_master.o

		arm-linux-gnueabihf-gcc test_master.o -shared -o testcases_master.so
		-------------------
	
		iii. For Standalone Mode
		------------------------
		test_standalone.c -> File containing test cases
	
		arm-linux-gnueabihf-gcc -c -fPIC test_standalone.c -o test_standalone.o

		arm-linux-gnueabihf-gcc test_standalone.o -shared -o testcases_standalone.so

---------------------------------------------------------------------------------------------

Section-1.4: Test framework execution
=============================================================================================

For testing any network driver the process is as follows:
---------------------------------------------------------
	1. Obtain the IP address of the master device. (eg. 192.168.1.2)
	
	2. Execute the test case on master device/supporting device as follows:
	
	   $./tframework Master testcases_master
	   (Note: don't mention the .so extension)
	   
	3. Note down the port number displayed
	
	4. Execute the test case on slave device/device under test as follows:
	
	   $./tframewoek Slave 192.168.1.2 testcases_slave
	   (Note: don't mention the .so extension)
	   
	5. Enter the port number that you have noted down in step 3.
	
	6. After execution test report will be generated on slave device in .xml format
	   (Test Report Name: Test_Report_<Timestamp>)
	
	7. Open the test report in any web browser.
	   (Note: test_framework.xsl should be copied in the same folder)
		
For testing any standalone application/device driver the process is as follows:
-------------------------------------------------------------------------------
	1. Execute the test case on master device/supporting device as follows:
	
	   $./tframework Standalone testcases_standalone
	   (Note: don't mention the .so extension)
	
	2. After execution test report will be generated on slave device in .xml format
	   (Test Report Name: Test_Report_<Timestamp>)
	
	3. Open the test report in any web browser.
	   (Note: test_framework.xsl should be copied in the same folder)

=============================================================================================

Section-1.5: How to add test cases and test suites?
=============================================================================================
Steps:

1. Create test case functions.
2. Make sure your test case function returns 1 for SUCCESS and 2 for FAILURE.
3. Create an array of testcases which will have a test case name, test case timeout and 
   address of test case function for each test case. 
   
   For example:

   /* test case function */
   int testcase(void)
   {
		//Do Something
		return 1;
   }
   
   /* test case array prototype*/
   struct TsTest_Case <array_name>[ARRAYSIZE] = 
									{"test_case_name", timeout, testcase function address, 
                              		 TEST_CASE_END}

	
   so array will be:
   struct TsTest_Case testcase[2] = {"TESTCASE_1",10 ,testcase, 
                              		  TEST_CASE_END};

   Note: Always mention TEST_CASE_END macro, which will be used by framework to know the test
		  cases are over

4. Pass the address of test case array in test suite array.
   For Example:
   
   /* test suite array prototype */
   struct TsTest_Suite aTestSuite[2] = {"test suite name", address of test case array,
                                       TEST_SUITE_END};

   Note: Do not change the name of the test suite array.

   so array will be:
   struct TsTest_Suite aTestSuite[2] = {"TESTSUIT_1", testcase,
                                       TEST_SUITE_END};
									   
   Note: Always mention TEST_SUITE_END macro, which will be used by framework to know the test
		  suites are over.

5. Create shared object as mentioned in Section-1.3 and execute the test cases as mentioned in
   Section-1.4.
   
Note: Create test case arrays based on the requirement. And add them to the aTestSuite array.
   
   
