import pandas as pd

A = 'acA1920-40gc.pfs'
B = 'acA1920-155uc.pfs'

A = pd.read_table(A, skiprows=3, header=None, names=['key', 'value'], delimiter='\t')
B = pd.read_table(B, skiprows=3, header=None, names=['key', 'value'], delimiter='\t')

print('GigE Only')
print('---------')
columns = sorted(list(set(A['key']) - set(B['key'])))
A[A['key'].isin(columns)]
for column in columns:
	print('Column: {}'.format(column))
	print(A[A['key'] == column])

print('USB Only')
print('--------')
columns = sorted(list(set(B['key']) - set(A['key'])))
B[B['key'].isin(columns)]
for column in columns:
	print('Column: {}'.format(column))
	print(B[B['key'] == column])

print('Both')
print('----')
columns = sorted(list(set(B['key']).intersection(set(A['key']))))
for column in columns:
	print('Column: {}'.format(column))
	print('GigE:')
	print(A[A['key'] == column])
	print('USB:')
	print(B[B['key'] == column])
	print(' = = = = = = = = ')
