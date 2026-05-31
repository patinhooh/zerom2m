#
#	testCIN.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	Modified by ZeroM2M Authors in 2026
#
#	Unit tests for CIN functionality
#

import unittest, sys, time
if '..' not in sys.path:
	sys.path.append('..')
from acmecse.etc.Types import ResourceTypes as T, ResponseStatusCode as RC
from acmecse.etc.DateUtils import getResourceDate
from init import *

class TestCIN(unittest.TestCase):

	ae 			= None
	cnt 		= None
	originator 	= None


	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def setUpClass(cls) -> None:
		testCaseStart('Setup TestCIN')
		# create AE with unique resource name to avoid collisions with other tests
		cls.ae_rn_local = f"{aeRN}{int(time.time()*1000)}"
		dct = 	{ 'm2m:ae' : {
				'rn'  : cls.ae_rn_local, 
					'api' : APPID,
				 	'rr'  : True,
				 	'srv' : [ RELEASEVERSION ]
				}}
		cls.ae, rsc = CREATE(cseURL, 'C', T.AE, dct) 	# AE to work under
		assert rsc == RC.CREATED, 'cannot create parent AE'
		cls.originator = findXPath(cls.ae, 'm2m:ae/aei')
		# derive AE URL from returned resource name so tests are independent of module-level names
		cls.ae_rn = findXPath(cls.ae, 'm2m:ae/rn')
		cls.aeURL_local = f"{cseURL}/{cls.ae_rn}"
		dct = 	{ 'm2m:cnt' : { 
				'rn'  : cntRN
			}}
		cls.cnt, rsc = CREATE(cls.aeURL_local, cls.originator, T.CNT, dct)
		assert rsc == RC.CREATED, 'cannot create container'
		# derive container URL (container rn is the requested cntRN)
		cls.cnt_rn = findXPath(cls.cnt, 'm2m:cnt/rn')
		cls.cntURL_local = f"{cls.aeURL_local}/{cls.cnt_rn}"
		cls.cinURL_local = f"{cls.cntURL_local}/{cinRN}"
		testCaseEnd('Setup TestCIN')


	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def tearDownClass(cls) -> None:
		if not isTearDownEnabled():
			return
		testCaseStart('TearDown TestCIN')
		testCaseEnd('TearDown TestCIN')



	def setUp(self) -> None:
		testCaseStart(self._testMethodName)
	

	def tearDown(self) -> None:
		testCaseEnd(self._testMethodName)

	
	#########################################################################
	

	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCIN(self) -> None:
		""" Create a <CIN> resource """
		self.assertIsNotNone(TestCIN.ae)
		self.assertIsNotNone(TestCIN.cnt)
		dct = 	{ 'm2m:cin' : {
					'rn'  : cinRN,
					'cnf' : 'text/plain:0',
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		# store created CIN rn and update class-local URL to use name-based path for reliable retrieval
		TestCIN.cin_rn = findXPath(r, 'm2m:cin/rn')
		TestCIN.cinURL_local = f"{TestCIN.cntURL_local}/{TestCIN.cin_rn}"


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCIN(self) -> None:
		""" Retrieve <CIN> resource """
		r, rsc = RETRIEVE(TestCIN.cinURL_local, TestCIN.originator)
		self.assertEqual(rsc, RC.OK, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_attributesCIN(self) -> None:
		""" Test <CIN> attributes """
		r, rsc = RETRIEVE(TestCIN.cinURL_local, TestCIN.originator)
		self.assertEqual(rsc, RC.OK, r)

		# TEST attributess
		self.assertEqual(findXPath(r, 'm2m:cin/ty'), T.CIN)
		self.assertEqual(findXPath(r, 'm2m:cin/pi'), findXPath(TestCIN.cnt,'m2m:cnt/ri'))
		self.assertEqual(findXPath(r, 'm2m:cin/rn'), cinRN)
		self.assertIsNotNone(findXPath(r, 'm2m:cin/ct'))
		self.assertIsNotNone(findXPath(r, 'm2m:cin/lt'))
		self.assertIsNotNone(findXPath(r, 'm2m:cin/et'))
		self.assertIsNotNone(findXPath(r, 'm2m:cin/st'))
		self.assertIsNone(findXPath(r, 'm2m:cin/cr'))
		self.assertIsNotNone(findXPath(r, 'm2m:cin/cnf'))
		self.assertEqual(findXPath(r, 'm2m:cin/cnf'), 'text/plain:0')
		self.assertIsNotNone(findXPath(r, 'm2m:cin/con'))
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 'AnyValue')
		self.assertGreater(findXPath(r, 'm2m:cin/cs'), 0)
		self.assertIsNone(findXPath(r, 'm2m:cin/acpi'))



	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINUnderAE(self) -> None:
		""" Create <CIN> resource under <AE> -> Fail """
		dct = 	{ 'm2m:cin' : {
					'rn'  : cinRN,
					'cnf' : 'text/plain:0',
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.aeURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.INVALID_CHILD_RESOURCE_TYPE, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINwithString(self) -> None:
		""" Create a <CIN> resource with string value """
		self.assertIsNotNone(TestCIN.ae)
		self.assertIsNotNone(TestCIN.cnt)
		dct = 	{ 'm2m:cin' : {
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 'AnyValue')	


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINwithInteger(self) -> None:
		""" Create a <CIN> resource with integer value """
		self.assertIsNotNone(TestCIN.ae)
		self.assertIsNotNone(TestCIN.cnt)
		dct = 	{ 'm2m:cin' : {
					'con' : 23
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 23)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINwithFloat(self) -> None:
		""" Create a <CIN> resource with float value """
		self.assertIsNotNone(TestCIN.ae)
		self.assertIsNotNone(TestCIN.cnt)
		dct = 	{ 'm2m:cin' : {
					'con' : 23.17
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 23.17)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINwithBoolean(self) -> None:
		""" Create a <CIN> resource with boolean value """
		self.assertIsNotNone(TestCIN.ae)
		self.assertIsNotNone(TestCIN.cnt)
		dct = 	{ 'm2m:cin' : {
					'con' : True
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), True)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINwithList(self) -> None:
		""" Create a <CIN> resource with list value """
		self.assertIsNotNone(TestCIN.ae)
		self.assertIsNotNone(TestCIN.cnt)
		dct = 	{ 'm2m:cin' : {
					'con' : [ 1, 2, 3, 4, 5 ]
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), [ 1, 2, 3, 4, 5 ])


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINwithStructure(self) -> None:
		""" Create a <CIN> resource with dict/JSON structure value """
		self.assertIsNotNone(TestCIN.ae)
		self.assertIsNotNone(TestCIN.cnt)
		dct = 	{ 'm2m:cin' : {
					'con' : { 'a': 1, 'b': 2, 'c': 3 }
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), { 'a': 1, 'b': 2, 'c': 3 })


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINWithCreatorWrong(self) -> None:
		""" Create <CIN> with creator attribute (wrong) -> Fail """
		dct = 	{ 'm2m:cin' : { 
					'cr' : 'wrong',
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)				# Not allowed
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINWithCnfWrong1(self) -> None:
		""" Create <CIN> with cnf attribute (wrong 1) -> Fail """
		dct = 	{ 'm2m:cin' : { 
					'cnf' : 'text',
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)				# Not allowed
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINWithCnfWrong2(self) -> None:
		""" Create <CIN> with cnf attribute (wrong 2) -> Fail """
		dct = 	{ 'm2m:cin' : { 
					'cnf' : 'text:0',
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)				# Not allowed
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINWithCnfWrong3(self) -> None:
		""" Create <CIN> with cnf attribute (wrong 4) -> Fail """
		dct = 	{ 'm2m:cin' : { 
					'cnf' : 'text/plain',
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)				# Not allowed
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINWithCnfWrong4(self) -> None:
		""" Create <CIN> with cnf attribute (wrong 5) -> Fail """
		dct = 	{ 'm2m:cin' : { 
					'cnf' : 'text/plain:0:0:0',
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)				# Not allowed
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINWithCnfWrong5(self) -> None:
		""" Create <CIN> with cnf attribute (wrong 6) -> Fail """
		dct = 	{ 'm2m:cin' : { 
					'cnf' : 'text/plain:9',
					'con' : 'AnyValue'
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)				# Not allowed
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINWithCreator(self) -> None:
		""" Create <CIN> with creator attribute set to Null """
		dct = 	{ 'm2m:cin' : { 
					'con' : 'AnyValue',
					'cr' : None
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)	
		self.assertEqual(rsc, RC.CREATED, r)
		self.assertEqual(findXPath(r, 'm2m:cin/cr'), TestCIN.originator)	# Creator should now be set to originator

		# Check whether creator is there in a RETRIEVE
		r, rsc = RETRIEVE(f"{TestCIN.cntURL_local}/{findXPath(r, 'm2m:cin/rn')}", TestCIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertEqual(findXPath(r, 'm2m:cin/cr'), TestCIN.originator)



	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINwithAcpi(self) -> None:
		""" Create a <CIN> with acpi attribute set -> Fail"""
		dct = 	{ 'm2m:cin' : { 
					'rn' : 'dcntTest',
					'con' : 'AnyValue',
					'acpi' : [ 'someACP' ]
				}}
		r, rsc = CREATE(cntURL, TestCIN.originator, T.CIN, dct)	
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINwithDgt(self) -> None:
		""" Create a <CIN> resource with dgt attribute"""
		self.assertIsNotNone(TestCIN.ae)
		self.assertIsNotNone(TestCIN.cnt)
		dgt = getResourceDate()
		dct = 	{ 'm2m:cin' : {
					'rn'  : f'{cinRN}dgt',
					'cnf' : 'text/plain:0',
					'con' : 'AnyValue',
					'dgt' : dgt
				}}
		r, rsc = CREATE(TestCIN.cntURL_local, TestCIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED, r)

		# RETRIEVE the CIN with the dgt
		r, rsc = RETRIEVE(f"{TestCIN.cntURL_local}/{cinRN}dgt", TestCIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertEqual(findXPath(r, 'm2m:cin/dgt'), dgt)



# More tests of la, ol etc in testCNT_CNI.py

def run(testFailFast:bool) -> TestResult:
	# Assign tests
	suite = unittest.TestSuite()
	addTests(suite, TestCIN, [
		'test_createCIN',
		'test_retrieveCIN',
		'test_attributesCIN',
		'test_createCINUnderAE',

		# Various content types
		'test_createCINwithString',
		'test_createCINwithInteger',
		'test_createCINwithFloat',
		'test_createCINwithBoolean',
		'test_createCINwithList',
		'test_createCINwithStructure',


		'test_createCINWithCreatorWrong',
		'test_createCINWithCnfWrong1',
		'test_createCINWithCnfWrong2',
		'test_createCINWithCnfWrong3',
		'test_createCINWithCnfWrong4',
		'test_createCINWithCnfWrong5',
		'test_createCINWithCreator',
		'test_createCINwithAcpi',
		'test_createCINwithDgt'
	])
	
	# Run tests
	result = unittest.TextTestRunner(verbosity=testVerbosity, failfast=testFailFast).run(suite)
	printResult(result)
	return result.testsRun, len(result.errors + result.failures), len(result.skipped), getSleepTimeCount()

if __name__ == '__main__':
	r, errors, s, t = run(True)
	sys.exit(errors)
