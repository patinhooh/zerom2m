#
#	testAE.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	Modified by ZeroM2M Authors in 2026
#
#	Unit tests for AE functionality
#

import unittest, sys
if '..' not in sys.path:
	sys.path.append('..')
from acmecse.etc.Types import ResourceTypes as T, ResponseStatusCode as RC
from init import *


class TestAE(unittest.TestCase):

	cse 		= None
	ae 			= None
	originator 	= None
	originator2	= None
	aeACPI 		= None
	aeURL 		= None

	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def setUpClass(cls) -> None:
		testCaseStart('Setup TestACP')
		cls.originator 	= None 	# actually the AE.aei
		cls.aeACPI 		= None
		cls.cse, rsc 	= RETRIEVE(cseURL, ORIGINATOR)
		assert rsc == RC.OK, f'Cannot retrieve CSEBase: {cseURL}'
		testCaseEnd('Setup TestAE')


	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def tearDownClass(cls) -> None:
		if not isTearDownEnabled():
			return
		testCaseStart('TearDown TestAE')
		testCaseEnd('TearDown TestAE')


	def setUp(self) -> None:
		testCaseStart(self._testMethodName)
	

	def tearDown(self) -> None:
		testCaseEnd(self._testMethodName)


	#########################################################################

	def _newAe(self, api:str, rvi:str|None = None, originator:str|None = ORIGINATORSelfReg) -> tuple[JSON, int, str]:
		"""Create an AE with a unique rn and return (resource, rsc, ae_url)."""
		rn = uniqueRN('testAE')
		dct = { 'm2m:ae' : {
				'rn': rn,
				'api': api,
				'rr': False,
				'srv': [ RELEASEVERSION ]
			}}
		headers = {}
		if rvi is not None:
			headers[C.hfRVI] = rvi
			ae, rsc = CREATE(cseURL, originator, T.AE, dct, headers = headers)
		else:
			ae, rsc = CREATE(cseURL, originator, T.AE, dct)
		return ae, rsc, f'{cseURL}/{rn}'


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAE(self) -> None:
		""" Create/register an <AE> """
		r, rsc, ae_url = self._newAe(APPID)

		self.assertEqual(rsc, RC.CREATED, r)
		TestAE.originator = findXPath(r, 'm2m:ae/aei')
		TestAE.aeACPI = findXPath(r, 'm2m:ae/acpi')
		TestAE.aeURL = ae_url
		self.assertIsNotNone(TestAE.originator, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEUnderAEFail(self) -> None:
		""" Create/register an <AE> under an <AE> -> Fail """
		rn = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
					'rn': rn,
					'api': APPID,
			 		'rr': False,
			 		'srv': [ RELEASEVERSION ]
				}}
		r, rsc = CREATE(TestAE.aeURL, ORIGINATORSelfReg, T.AE, dct)
		self.assertEqual(rsc, RC.INVALID_CHILD_RESOURCE_TYPE, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEAgainFail(self) -> None:
		""" Create/register an <AE> with same rn again -> Fail """
		rn = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
					'rn': rn,
					'api': APPID,
				 	'rr': False,
				 	'srv': [ RELEASEVERSION ]
				}}
		_, rsc = CREATE(cseURL, ORIGINATORSelfReg, T.AE, dct)
		self.assertEqual(rsc, RC.CREATED)
		_, rsc = CREATE(cseURL, ORIGINATORSelfReg, T.AE, dct)
		self.assertEqual(rsc, RC.CONFLICT)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEWithExistingOriginatorFail(self) -> None:
		""" Create/register an <AE> with same originator again -> Fail """
		dct = 	{ 'm2m:ae' : {
					'api': APPID,
				 	'rr': False,
				 	'srv': [ RELEASEVERSION ]
				}}
		r, rsc = CREATE(cseURL, TestAE.originator, T.AE, dct)
		self.assertEqual(rsc, RC.ORIGINATOR_HAS_ALREADY_REGISTERED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAECSIoriginatorFail(self) -> None:
		""" Create/register an <AE> with the CSI originator -> Fail """
		dct = 	{ 'm2m:ae' : {
					'api': APPID,
				 	'rr': False,
				 	'srv': [ RELEASEVERSION ]
				}}
		r, rsc = CREATE(cseURL, CSEID[1:], T.AE, dct)
		self.assertEqual(rsc, RC.SECURITY_ASSOCIATION_REQUIRED, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveAE(self) -> None:
		""" Retrieve <AE> """
		r, rsc = RETRIEVE(TestAE.aeURL, TestAE.originator)
		self.assertEqual(rsc, RC.OK, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveAEWithWrongOriginator(self) -> None:
		""" Retrieve <AE> with wrong originator -> Fail """
		_, rsc = RETRIEVE(TestAE.aeURL, 'Cwrong')
		self.assertIn(rsc, [RC.ORIGINATOR_HAS_NO_PRIVILEGE, RC.SERVICE_SUBSCRIPTION_NOT_ESTABLISHED])


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_attributesAE(self) -> None:
		""" Retrieve <AE> and check attributes """
		r, rsc = RETRIEVE(TestAE.aeURL, TestAE.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(findXPath(r, 'm2m:ae/aei'))
		self.assertEqual(findXPath(r, 'm2m:ae/ty'), T.AE)
		self.assertTrue(findXPath(r, 'm2m:ae/aei').startswith('C'))
		self.assertEqual(findXPath(r, 'm2m:ae/api'), 'NMyApp1Id')
		self.assertIsNotNone(findXPath(r, 'm2m:ae/ct'))
		self.assertIsNotNone(findXPath(r, 'm2m:ae/lt'))
		self.assertIsNotNone(findXPath(r, 'm2m:ae/et'))
		self.assertLessEqual(findXPath(r, 'm2m:ae/ct'), findXPath(r, 'm2m:ae/lt'))
		self.assertLess(findXPath(r, 'm2m:ae/ct'), findXPath(r, 'm2m:ae/et'))
		self.assertEqual(findXPath(r, 'm2m:ae/rr'), False)
		self.assertIsNotNone(findXPath(r, 'm2m:ae/srv'))
		self.assertEqual(findXPath(r, 'm2m:ae/srv'), [ RELEASEVERSION ])
		self.assertIsNone(findXPath(r, 'm2m:ae/st'))
		self.assertEqual(findXPath(r, 'm2m:ae/pi'), findXPath(TestAE.cse,'m2m:cb/ri'))
		#self.assertIsNotNone(findXPath(r, 'm2m:ae/acpi'))
		#self.assertIsInstance(findXPath(r, 'm2m:ae/acpi'), list)
		#self.assertGreater(len(findXPath(r, 'm2m:ae/acpi')), 0)
		self.assertIsNone(findXPath(r, 'm2m:ae/st'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAENoAPI(self) -> None:
		""" Create <AE> with missing api attribute -> Fail"""
		rn = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
					'rn': rn,
			 		'rr': False,
			 		'srv': [ RELEASEVERSION ]
				}}
		r, rsc = CREATE(cseURL, ORIGINATORSelfReg, T.AE, dct)

		self.assertNotEqual(rsc, RC.CREATED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEAPIWrongPrefix(self) -> None:
		""" Create <AE> with unknown api prefix -> Fail"""
		rn = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
					'rn': rn,
					'api': 'Xwrong',
			 		'rr': False,
			 		'srv': [ RELEASEVERSION ]
				}}
		r, rsc = CREATE(cseURL, ORIGINATORSelfReg, T.AE, dct)

		self.assertNotEqual(rsc, RC.CREATED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEAPICorrectR(self) -> None:
		""" Create <AE> with correct api value (Registered)"""
		ae, rsc, _ = self._newAe('Rabc.com.example.acme')

		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(ae, 'm2m:ae/aei'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEAPICorrectN(self) -> None:
		""" Create <AE> with correct api value (Non-Registered)"""
		ae, rsc, _ = self._newAe('Nacme')

		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(ae, 'm2m:ae/aei'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEAPIRVI3LowerCaseR(self) -> None:
		""" Create <AE> with RVI=3 and lower case API"""
		ae, rsc, _ = self._newAe('racme', rvi = '3')

		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(ae, 'm2m:ae/aei'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEAPIRVI4LowerCaseRFail(self) -> None:
		""" Create <AE> with RVI=4 and lower case API -> Fail"""
		rn = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
					'rn': rn,
					'api': 'racme',
			 		'rr': False,
			 		'srv': [ '4' ]	# explicit 4
				}}
		headers={ C.hfRVI: '4'	# explicit 4
		}
		ae, rsc = CREATE(cseURL, ORIGINATORSelfReg, T.AE, dct, headers = headers)

		self.assertEqual(rsc, RC.BAD_REQUEST, ae)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAENoOriginator(self) -> None:
		""" Create <AE> without an Originator"""
		rn = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
					'rn': rn,
					'api': 'Nacme',
			 		'rr': False,
			 		'srv': [ RELEASEVERSION ]
				}}
		ae, rsc = CREATE(cseURL, None, T.AE, dct)
		self.assertEqual(rsc, RC.CREATED, ae)
		self.assertIsNotNone(findXPath(ae, 'm2m:ae/aei'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEEmptyOriginator(self) -> None:
		""" Create <AE> with an empty Originator"""
		rn = uniqueRN('testAE')
		dct = 	{ 'm2m:ae' : {
					'rn': rn,
					'api': 'Nacme',
			 		'rr': False,
			 		'srv': [ RELEASEVERSION ]
				}}
		ae, rsc = CREATE(cseURL, ORIGINATOREmpty, T.AE, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(ae, 'm2m:ae/aei'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAEInvalidRNFail(self) -> None:
		""" Create <AE> with an invalid rn -> Fail"""
		
		# With unallowed character
		dct = 	{ 'm2m:ae' : {
					'rn': 'test?',	# not from unreserved character
					'api': 'Nacme',
			 		'rr': False,
			 		'srv': [ RELEASEVERSION ]
				}}
		ae, rsc = CREATE(cseURL, ORIGINATOREmpty, T.AE, dct)
		self.assertEqual(rsc, RC.BAD_REQUEST)

		# With space
		dct = 	{ 'm2m:ae' : {
					'rn': 'test wrong',	# not from unreserved character
					'api': 'Nacme',
			 		'rr': False,
			 		'srv': [ RELEASEVERSION ]
				}}
		ae, rsc = CREATE(cseURL, ORIGINATOREmpty, T.AE, dct)
		self.assertEqual(rsc, RC.BAD_REQUEST)



# TODO register multiple AEs
# TODO register with S


def run(testFailFast:bool) -> TestResult:
	# Assign tests
	suite = unittest.TestSuite()
	addTests(suite, TestAE, [
		'test_createAE',
		'test_createAEUnderAEFail',
		'test_createAEAgainFail',
		'test_createAEWithExistingOriginatorFail',
		'test_createAECSIoriginatorFail',
		'test_retrieveAE',
		'test_retrieveAEWithWrongOriginator',
		'test_attributesAE',
		'test_createAENoAPI',
		'test_createAEAPIWrongPrefix',
		'test_createAEAPICorrectR',
		'test_createAEAPICorrectN',
		'test_createAEAPIRVI3LowerCaseR',
		'test_createAEAPIRVI4LowerCaseRFail',
		'test_createAENoOriginator',
		'test_createAEEmptyOriginator',
		'test_createAEInvalidRNFail',
	])

	# Run tests
	result = unittest.TextTestRunner(verbosity=testVerbosity, failfast=testFailFast).run(suite)
	printResult(result)
	return result.testsRun, len(result.errors + result.failures), len(result.skipped), getSleepTimeCount()

if __name__ == '__main__':
	r, errors, s, t = run(True)
	sys.exit(errors)
