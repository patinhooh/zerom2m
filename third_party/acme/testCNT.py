#
#	testCNT.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	Modified by ZeroM2M Authors in 2026
#
#	Unit tests for CNT functionality
#

import unittest, sys
if '..' not in sys.path:
	sys.path.append('..')
from acmecse.etc.Types import ResourceTypes as T, ResponseStatusCode as RC
from init import *


class TestCNT(unittest.TestCase):

	ae 				= None
	originator 		= None
	aeURL 			= None
	aeRN 			= None
	cntURL 			= None
	cntRN 			= None
	childCntURL 	= None
	cseCntURL 		= None

	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def setUpClass(cls) -> None:
		testCaseStart('Setup testCNT')
		cls.aeRN = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
					'rn': cls.aeRN,
					'api': APPID,
				 	'rr': False,
				 	'srv': [ RELEASEVERSION ]
				}}
		cls.ae, rsc = CREATE(cseURL, 'C', T.AE, dct)	# AE to work under
		assert rsc == RC.CREATED, 'cannot create parent AE'
		cls.originator = findXPath(cls.ae, 'm2m:ae/aei')
		cls.aeURL = f'{cseURL}/{cls.aeRN}'
		testCaseEnd('Setup testCNT')


	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def tearDownClass(cls) -> None:
		if not isTearDownEnabled():
			return
		testCaseStart('TearDown testCNT')
		testCaseEnd('TearDown testCNT')


	def setUp(self) -> None:
		testCaseStart(self._testMethodName)
	

	def tearDown(self) -> None:
		testCaseEnd(self._testMethodName)


	#########################################################################


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNT(self) -> None:
		"""	Create <CNT> """
		self.assertIsNotNone(TestCNT)
		self.assertIsNotNone(TestCNT.ae)
		TestCNT.cntRN = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : { 
					'rn' : TestCNT.cntRN
				}}
		r, rsc = CREATE(TestCNT.aeURL, TestCNT.originator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		TestCNT.cntURL = f'{TestCNT.aeURL}/{TestCNT.cntRN}'


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCNT(self) -> None:
		""" Retrieve <CNT> """
		_, rsc = RETRIEVE(TestCNT.cntURL, TestCNT.originator)
		self.assertEqual(rsc, RC.OK)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCNTWithWrongOriginator(self) -> None:
		"""	Retrieve <CNT> with wrong originator -> Fail """
		_, rsc = RETRIEVE(TestCNT.cntURL, 'Cwrong')
		self.assertEqual(rsc, RC.ORIGINATOR_HAS_NO_PRIVILEGE)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_attributesCNT(self) -> None:
		""" Test <CNT> attributes """
		r, rsc = RETRIEVE(TestCNT.cntURL, TestCNT.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertEqual(findXPath(r, 'm2m:cnt/ty'), T.CNT)
		self.assertEqual(findXPath(r, 'm2m:cnt/pi'), findXPath(TestCNT.ae,'m2m:ae/ri'))
		self.assertEqual(findXPath(r, 'm2m:cnt/rn'), TestCNT.cntRN)
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/ct'))
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/lt'))
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/et'))
		self.assertIsNotNone(findXPath(r, 'm2m:cnt/st'))
		self.assertIsNone(findXPath(r, 'm2m:cnt/cr'))
		self.assertEqual(findXPath(r, 'm2m:cnt/cbs'), 0)
		self.assertEqual(findXPath(r, 'm2m:cnt/cni'), 0)
		self.assertIsNone(findXPath(r, 'm2m:cnt/lbl'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNTUnderCNT(self) -> None:
		""" Create <CNT> under <CNT> """
		child_rn = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : { 
					'rn' : child_rn
				}}
		r, rsc = CREATE(TestCNT.cntURL, TestCNT.originator, T.CNT, dct) 
		self.assertEqual(rsc, RC.CREATED)
		TestCNT.childCntURL = f'{TestCNT.cntURL}/{child_rn}'


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCNTUnderCNT(self) -> None:
		"""	Retrieve <CNT> under <CNT> """
		_, rsc = RETRIEVE(TestCNT.childCntURL, TestCNT.originator)
		self.assertEqual(rsc, RC.OK)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNTWithCreatorWrong(self) -> None:
		""" Create <CNT> with creator attribute (wrong) -> Fail """
		dct = 	{ 'm2m:cnt' : { 
					'cr' : 'wrong'
				}}
		r, rsc = CREATE(TestCNT.aeURL, TestCNT.originator, T.CNT, dct)
		self.assertEqual(rsc, RC.BAD_REQUEST)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNTWithCreator(self) -> None:
		""" Create <CNT> with creator attribute set to Null """
		dct = 	{ 'm2m:cnt' : { 
					'cr' : None
				}}
		r, rsc = CREATE(TestCNT.aeURL, TestCNT.originator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertEqual(findXPath(r, 'm2m:cnt/cr'), TestCNT.originator, r)	# Creator should now be set to originator

		# Check whether creator is there in a RETRIEVE
		r, rsc = RETRIEVE(f'{TestCNT.aeURL}/{findXPath(r, "m2m:cnt/rn")}', TestCNT.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertEqual(findXPath(r, 'm2m:cnt/cr'), TestCNT.originator)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNTUnderCSE(self) -> None:
		"""	Create <CNT> under <CB> with admin Originator """
		cse_rn = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : { 
					'rn' : cse_rn
				}}
		r, rsc = CREATE(cseURL, ORIGINATOR, T.CNT, dct) # With Admin originator !!
		self.assertEqual(rsc, RC.CREATED)
		TestCNT.cseCntURL = f'{cseURL}/{cse_rn}'


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCNTUnderCSE(self) -> None:
		"""	Retrieve <CNT> under <CB> with admin Originator """
		_, rsc = RETRIEVE(TestCNT.cseCntURL, ORIGINATOR)
		self.assertEqual(rsc, RC.OK)


	@unittest.skipIf(noCSE, 'No CSEBase')
	@unittest.skipUnless(BINDING in [ 'http', 'https' ], 'Only when testing with http(s) binding')
	def test_createCNTWithoutOriginator(self) -> None:
		"""	Create <CNT> under <CB> without an Originator -> Fail"""
		no_origin_rn = uniqueRN('testCNT')
		dct = 	{ 'm2m:cnt' : { 
					'rn' : no_origin_rn
				}}
		r, rsc = CREATE(cseURL, None, T.CNT, dct) # Without originator !!
		self.assertNotEqual(rsc, RC.CREATED)

	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNTwithWrongTypeShortname(self) -> None:
		"""	Create <CNT> with wrong typeShortname -> Fail"""
		wrong_rn = uniqueRN('testCNT')
		dct = 	{ 'wrong' : { 
					'rn' : wrong_rn
				}}
		r, rsc = CREATE(cseURL, ORIGINATOR, T.CNT, dct) # Without originator !!
		self.assertNotEqual(rsc, RC.CREATED)


def run(testFailFast:bool) -> TestResult:

	# Assign tests
	suite = unittest.TestSuite()
	addTests(suite, TestCNT, [
				
		'test_createCNT',
		'test_retrieveCNT',
		'test_retrieveCNTWithWrongOriginator',
		'test_attributesCNT',
		'test_createCNTUnderCNT',
		'test_retrieveCNTUnderCNT',
		'test_createCNTWithCreatorWrong',
		'test_createCNTWithCreator',
		'test_createCNTUnderCSE',
		'test_retrieveCNTUnderCSE',
		'test_createCNTWithoutOriginator',
		'test_createCNTwithWrongTypeShortname',
	
	])

	# Run the tests
	result = unittest.TextTestRunner(verbosity=testVerbosity, failfast=testFailFast).run(suite)
	printResult(result)
	return result.testsRun, len(result.errors + result.failures), len(result.skipped), getSleepTimeCount()


if __name__ == '__main__':
	r, errors, s, t = run(True)
	sys.exit(errors)
