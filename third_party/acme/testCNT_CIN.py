#
#	testCNT_CIN.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	Modified by ZeroM2M Authors in 2026
#
#	Unit tests for CNT & CIN functionality
#

import unittest, sys
if '..' not in sys.path:
	sys.path.append('..')
from acmecse.etc.Types import NotificationEventType, ResourceTypes as T, ResponseStatusCode as RC, ResultContentType
from init import *


maxBS = 30
testValue = 'aValue'

class TestCNT_CIN(unittest.TestCase):

	ae 			= None
	originator 	= None
	aeRN			= None
	aeURL			= None
	cnt 			= None
	cntRN			= None
	cntURL			= None

	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def setUpClass(cls) -> None:
		testCaseStart('Setup TestCNT_CIN')
		# Create a unique AE for this test class to avoid conflicts with other tests
		cls.aeRN = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
				'rn'  : cls.aeRN,
				'api' : APPID,
				'rr'  : True,
				'srv' : [ RELEASEVERSION ]
			}}
		cls.ae, rsc = CREATE(cseURL, 'C', T.AE, dct) 	# AE to work under
		assert rsc == RC.CREATED, 'cannot create parent AE'
		cls.originator = findXPath(cls.ae, 'm2m:ae/aei')
		cls.aeURL = f'{cseURL}/{cls.aeRN}'
		# Create a unique container for this test class
		cls.cntRN = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : {
				'rn'  : cls.cntRN,
				'mni' : 3
			}}
		cls.cnt, rsc = CREATE(cls.aeURL, cls.originator, T.CNT, dct)
		assert rsc == RC.CREATED, 'cannot create container'
		assert findXPath(cls.cnt, 'm2m:cnt/mni') == 3, 'mni is not correct'
		cls.cntURL = f'{cls.aeURL}/{cls.cntRN}'

		# Start notification server
		startNotificationServer()
		# look for notification server
		assert isNotificationServerRunning(), 'Notification server cannot be reached'
		testCaseEnd('Setup TestCNT_CIN')


	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def tearDownClass(cls) -> None:
		if not isTearDownEnabled():
			return
		testCaseStart('TearDown TestCNT_CIN')
		testCaseEnd('TearDown TestCNT_CIN')


	def setUp(self) -> None:
		testCaseStart(self._testMethodName)
	

	def tearDown(self) -> None:
		testCaseEnd(self._testMethodName)


	#########################################################################


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_addCIN(self) -> None:
		"""\tCreate <CIN> under <CNT> """
		self.assertIsNotNone(TestCNT_CIN.ae)
		self.assertIsNotNone(TestCNT_CIN.cnt)
		dct = 	{ 'm2m:cin' : {
					'cnf' : 'text/plain:0',
					'con' : testValue
				}}
		r, rsc = CREATE(TestCNT_CIN.cntURL, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(r)
		self.assertIsNotNone(findXPath(r, 'm2m:cin/ri'))
		self.assertEqual(findXPath(r, 'm2m:cin/con'), testValue)
		self.assertEqual(findXPath(r, 'm2m:cin/cnf'), 'text/plain:0')
		self.cinARi = findXPath(r, 'm2m:cin/ri')			# store ri

		r, rsc = RETRIEVE(TestCNT_CIN.cntURL, TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/cni'))
		self.assertIsInstance(findXPath(r, 'm2m:cnt/cni'), int)
		self.assertEqual(findXPath(r, 'm2m:cnt/cni'), 1)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_addMoreCIN(self) -> None:
		"""\tCreate more <CIN>s under <CNT> """
		dct = 	{ 'm2m:cin' : {
					'cnf' : 'text/plain:0',
					'con' : 'bValue'
				}}
		r, rsc = CREATE(TestCNT_CIN.cntURL, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 'bValue')

		r, rsc = RETRIEVE(TestCNT_CIN.cntURL, TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/cni'))
		self.assertIsInstance(findXPath(r, 'm2m:cnt/cni'), int)
		self.assertEqual(findXPath(r, 'm2m:cnt/cni'), 2)

		dct = 	{ 'm2m:cin' : {
					'cnf' : 'text/plain:0',
					'con' : 'cValue'
				}}
		r, rsc = CREATE(TestCNT_CIN.cntURL, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 'cValue')

		r, rsc = RETRIEVE(TestCNT_CIN.cntURL, TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/cni'))
		self.assertIsInstance(findXPath(r, 'm2m:cnt/cni'), int)
		self.assertEqual(findXPath(r, 'm2m:cnt/cni'), 3)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCNTLa(self) -> None:
		"""\tRetrieve <CNT>.LA """
		r, rsc = RETRIEVE(f'{TestCNT_CIN.cntURL}/la', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK, r)
		self.assertIsNotNone(r)
		self.assertEqual(findXPath(r, 'm2m:cin/ty'), T.CIN)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 'cValue')


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCNTOl(self) -> None:
		""" Retrieve <CNT>.OL """
		r, rsc = RETRIEVE(f'{TestCNT_CIN.cntURL}/ol', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(r)
		self.assertEqual(findXPath(r, 'm2m:cin/ty'), T.CIN)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 'aValue')


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNTwithMBS(self) -> None:
		"""\tCreate <CNT> with mbs"""
		unique_cnt_rn = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : { 
					'rn'  : unique_cnt_rn,
					'mbs' : maxBS
				}}
		TestCNT_CIN.cnt_with_mbs, rsc = CREATE(TestCNT_CIN.aeURL, TestCNT_CIN.originator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED)
		# set URL for the newly created container
		TestCNT_CIN.cnt_with_mbs_url = f'{TestCNT_CIN.aeURL}/{unique_cnt_rn}'
		self.assertIsNotNone(findXPath(TestCNT_CIN.cnt_with_mbs, 'm2m:cnt/mbs'))
		self.assertEqual(findXPath(TestCNT_CIN.cnt_with_mbs, 'm2m:cnt/mbs'), maxBS)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINexactSize(self) -> None:
		"""\tAdd <CIN> to <CNT> with exact max size"""
		dct = 	{ 'm2m:cin' : {
					'con' : 'x' * maxBS
				}}
		_, rsc = CREATE(TestCNT_CIN.cnt_with_mbs_url, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINtooBig(self) -> None:
		"""\tAdd <CIN> to <CNT> with size > mbs -> Fail """
		dct = 	{ 'm2m:cin' : {
					'con' : 'x' * (maxBS + 1)
				}}
		_, rsc = CREATE(TestCNT_CIN.cnt_with_mbs_url, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.NOT_ACCEPTABLE)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCINsForCNTwithSize(self) -> None:
		"""\tAdd multiple <CIN>s to <CNT> with size restrictions """
		unique_cnt_rn = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : {
					'rn'  : unique_cnt_rn,
					'mbs' : maxBS
				}}
		TestCNT_CIN.cnt_with_mbs, rsc = CREATE(TestCNT_CIN.aeURL, TestCNT_CIN.originator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED)
		# set URL for the newly created container
		TestCNT_CIN.cnt_with_mbs_url = f'{TestCNT_CIN.aeURL}/{unique_cnt_rn}'

		dct = 	{ 'm2m:cin' : {
				'con' : 'x' * int(maxBS / 3)
			}}
		_, rsc = CREATE(TestCNT_CIN.cnt_with_mbs_url, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)

		dct = 	{ 'm2m:cin' : {
				'con' : 'x' * int(maxBS / 3)
			}}
		_, rsc = CREATE(TestCNT_CIN.cnt_with_mbs_url, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)


		# Test latest CIN for x
		r, rsc = RETRIEVE(f'{TestCNT_CIN.cnt_with_mbs_url}/la', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(findXPath(r, 'm2m:cin/con'))
		self.assertTrue(findXPath(r, 'm2m:cin/con').startswith('x'))
		self.assertEqual(len(findXPath(r, 'm2m:cin/con')), int(maxBS / 3))

		# Add another CIN
		dct = 	{ 'm2m:cin' : {
					'con' : 'y' * int(maxBS / 3)
				}}
		_, rsc = CREATE(TestCNT_CIN.cnt_with_mbs_url, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)

		# Test latest CIN for y
		r, rsc = RETRIEVE(f'{TestCNT_CIN.cnt_with_mbs_url}/la', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(findXPath(r, 'm2m:cin/con'))
		self.assertTrue(findXPath(r, 'm2m:cin/con').startswith('y'))
		self.assertEqual(len(findXPath(r, 'm2m:cin/con')), int(maxBS / 3))

		# Test CNT
		r, rsc = RETRIEVE(TestCNT_CIN.cnt_with_mbs_url, TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/cni'))
		self.assertEqual(findXPath(r, 'm2m:cnt/cni'), 3)
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/cbs'))
		self.assertEqual(findXPath(r, 'm2m:cnt/cbs'), maxBS)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNTwithDISR(self) -> None:
		""" Create <CNT> with disr = True and add <CIN>"""
		unique_cnt_rn = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : { 
					'rn'  : unique_cnt_rn,
					'disr' : True
				}}
		TestCNT_CIN.disr_cnt, rsc = CREATE(TestCNT_CIN.aeURL, TestCNT_CIN.originator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(TestCNT_CIN.disr_cnt, 'm2m:cnt/disr'))
		self.assertEqual(findXPath(TestCNT_CIN.disr_cnt, 'm2m:cnt/disr'), True)
		# set URL for disr container and add CINs under it
		TestCNT_CIN.disrCntURL = f'{TestCNT_CIN.aeURL}/{unique_cnt_rn}'
		for i in range(5):
			dct = 	{ 'm2m:cin' : {
					'rn'  : f'{i}',
					'con' : f'{i}',
				}}
			_, rsc = CREATE(TestCNT_CIN.disrCntURL, TestCNT_CIN.originator, T.CIN, dct)
			self.assertEqual(rsc, RC.CREATED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCINwithDISRFail(self) -> None:
		""" Retrieve <CIN> with disr = True -> FAIL """
		r, rsc = RETRIEVE(f'{TestCNT_CIN.disrCntURL}/3', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OPERATION_NOT_ALLOWED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveLAwithDISRFail(self) -> None:
		""" Retrieve <CNT>.LA with disr = True -> FAIL """
		r, rsc = RETRIEVE(f'{TestCNT_CIN.disrCntURL}/la', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OPERATION_NOT_ALLOWED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveOLwithDISRFail(self) -> None:
		""" Retrieve <CNT>.OL with disr = True -> FAIL """
		r, rsc = RETRIEVE(f'{TestCNT_CIN.disrCntURL}/ol', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OPERATION_NOT_ALLOWED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_discoverCINwithDISRFail(self) -> None:
		""" Discover <CIN> with disr = True -> FAIL """
		r, rsc = RETRIEVE(f'{TestCNT_CIN.disrCntURL}?rcn={int(ResultContentType.childResourceReferences)}', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OPERATION_NOT_ALLOWED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCINwithDISRAllowed(self) -> None:
		""" Retrieve <CIN> with disr = False"""
		unique_cnt_rn = uniqueRN('testCNT')
		dct = { 'm2m:cnt' : {
				'rn'  : unique_cnt_rn,
				'disr': False,
			}}
		cnt, rsc = CREATE(TestCNT_CIN.aeURL, TestCNT_CIN.originator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED, cnt)
		cntURL = f'{TestCNT_CIN.aeURL}/{unique_cnt_rn}'

		dct = { 'm2m:cin' : {
				'rn'  : '3',
				'con' : '3',
			}}
		_, rsc = CREATE(cntURL, TestCNT_CIN.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)

		r, rsc = RETRIEVE(f'{cntURL}/3', TestCNT_CIN.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 3)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNT5CIN(self) -> None:
		""" Create <CNT> and 5 <CIN> """
		# Create <CNT>
		unique_cnt_rn = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : { 
					'rn'  : unique_cnt_rn,
					'mni' : 10
				}}
		TestCNT_CIN.cnt5, rsc = CREATE(TestCNT_CIN.aeURL, TestCNT_CIN.originator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED, TestCNT_CIN.cnt5)
		# add CINs to the newly created container
		TestCNT_CIN.cnt5URL = f'{TestCNT_CIN.aeURL}/{unique_cnt_rn}'
		dct = 	{ 'm2m:cin' : {
				'cnf' : 'text/plain:0',
				'con' : testValue
			}}
		for _ in range(5):
			r, rsc = CREATE(TestCNT_CIN.cnt5URL, TestCNT_CIN.originator, T.CIN, dct)
			self.assertEqual(rsc, RC.CREATED, r)


def run(testFailFast:bool) -> TestResult:

	# Assign tests
	suite = unittest.TestSuite()
	addTests(suite, TestCNT_CIN, [
			
		'test_addCIN',
		'test_addMoreCIN',
		'test_retrieveCNTLa',
		'test_retrieveCNTOl',
		'test_createCNTwithMBS',
		'test_createCINexactSize',
		'test_createCINtooBig',
		'test_createCINsForCNTwithSize',
		'test_createCNTwithDISR',
		'test_retrieveCINwithDISRFail',
		'test_retrieveLAwithDISRFail',
		'test_retrieveOLwithDISRFail',
		'test_discoverCINwithDISRFail',
		'test_retrieveCINwithDISRAllowed',
		'test_createCNT5CIN',
	])

	# Run the tests
	result = unittest.TextTestRunner(verbosity=testVerbosity, failfast=testFailFast).run(suite)
	printResult(result)
	return result.testsRun, len(result.errors + result.failures), len(result.skipped), getSleepTimeCount()


if __name__ == '__main__':
	r, errors, s, t = run(True)
	sys.exit(errors)
